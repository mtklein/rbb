#include "rbb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

struct rbb {
    int             insts,in,out,regs;
    void           *jit_trampoline_f8;
    void           *jit_kernel_f8;
    size_t          jit_size_f8;
    void           *jit_trampoline_f4;
    void           *jit_kernel_f4;
    size_t          jit_size_f4;
    void           *jit_trampoline_h8;
    void           *jit_kernel_h8;
    size_t          jit_size_h8;
    struct rbb_inst inst[];
};

static int arity(enum rbb_op op) {
    return op < NEG ? 0 :
           op < ADD ? 1 :
           op < FMA ? 2 : 3;
}

static size_t jit_inst_size_f8(struct rbb_inst const *ip) {
    if (ip->op == CALL) {
        // ADRP+ADD+BLR is always 12 bytes.  We additionally need:
        //   - frame + saves + restores for caller's V[0..2*min(d,cregs)-1]
        //     (the chunks above the call frame; in-frame chunks are either
        //      callee inputs we're consuming or callee scratch we don't read back).
        //   - input/output MOVs only when d>0; for d==0 they are identity MOVs.
        struct rbb const *callee = ip->call;
        int const d = ip->d;
        int const save_count = d < callee->regs ? d : callee->regs;
        size_t bytes = 12;
        if (save_count > 0) { bytes += (size_t)(8 + 8*save_count); }
        if (d          > 0) { bytes += (size_t)(8*(callee->in + callee->out)); }
        return bytes;
    }
    return ip->op == IMM ? 16 : 8;
}

static size_t jit_inst_size_h8(struct rbb_inst const *ip) {
    if (ip->op == CALL) {
        struct rbb const *callee = ip->call;
        int const d = ip->d;
        int const save_count = d < callee->regs ? d : callee->regs;
        size_t bytes = 12;
        if (save_count > 0) { bytes += (size_t)(8 + 8*save_count); }
        if (d          > 0) { bytes += (size_t)(4*(callee->in + callee->out)); }
        return bytes;
    }
    return ip->op == IMM ? 8 : 4;
}

static size_t jit_inst_size_f4(struct rbb_inst const *ip) {
    if (ip->op == CALL) {
        struct rbb const *callee = ip->call;
        int const d = ip->d;
        int const save_count = d < callee->regs ? d : callee->regs;
        size_t bytes = 12;
        if (save_count > 0) { bytes += (size_t)(8 + 8*save_count); }
        if (d          > 0) { bytes += (size_t)(4*(callee->in + callee->out)); }
        return bytes;
    }
    return ip->op == IMM ? 12 : 4;
}

static uint32_t const enc_op_f[] = {
    [ADD]   = 0x4E20D400,  // FADD .4S
    [SUB]   = 0x4EA0D400,  // FSUB .4S
    [MUL]   = 0x6E20DC00,  // FMUL .4S
    [DIV]   = 0x6E20FC00,  // FDIV .4S
    [MIN]   = 0x4EA0F400,  // FMIN .4S
    [MAX]   = 0x4E20F400,  // FMAX .4S
    [NEG]   = 0x6EA0F800,  // FNEG   .4S
    [ABS]   = 0x4EA0F800,  // FABS   .4S
    [SQRT]  = 0x6EA1F800,  // FSQRT  .4S
    [FLOOR] = 0x4E219800,  // FRINTM .4S  (round toward -inf)
    [CEIL]  = 0x4EA18800,  // FRINTP .4S  (round toward +inf)
    [TRUNC] = 0x4EA19800,  // FRINTZ .4S  (round toward zero)
    [ROUND] = 0x6E218800,  // FRINTA .4S  (round to nearest, ties away)
    [AND]   = 0x4E201C00,  // AND  .16B
    [OR]    = 0x4EA01C00,  // ORR  .16B
    [XOR]   = 0x6E201C00,  // EOR  .16B
    [NOT]   = 0x6E205800,  // NOT  .16B (alias of MVN)
    [EQ]    = 0x4E20E400,  // FCMEQ .4S
    [GE]    = 0x6E20E400,  // FCMGE .4S
    [GT]    = 0x6EA0E400,  // FCMGT .4S
    [FMA]   = 0x4E20CC00,  // FMLA  .4S
};
static uint32_t const enc_op_h[] = {
    [ADD]   = 0x4E401400,  // FADD   .8H
    [SUB]   = 0x4EC01400,  // FSUB   .8H
    [MUL]   = 0x6E401C00,  // FMUL   .8H
    [DIV]   = 0x6E403C00,  // FDIV   .8H
    [MIN]   = 0x4EC03400,  // FMIN   .8H
    [MAX]   = 0x4E403400,  // FMAX   .8H
    [NEG]   = 0x6EF8F800,  // FNEG   .8H
    [ABS]   = 0x4EF8F800,  // FABS   .8H
    [SQRT]  = 0x6EF9F800,  // FSQRT  .8H
    [FLOOR] = 0x4E799800,  // FRINTM .8H
    [CEIL]  = 0x4EF98800,  // FRINTP .8H
    [TRUNC] = 0x4EF99800,  // FRINTZ .8H
    [ROUND] = 0x6E798800,  // FRINTA .8H
    [AND]   = 0x4E201C00,  // AND  .16B
    [OR]    = 0x4EA01C00,  // ORR  .16B
    [XOR]   = 0x6E201C00,  // EOR  .16B
    [NOT]   = 0x6E205800,  // NOT  .16B
    [EQ]    = 0x4E402400,  // FCMEQ .8H
    [GE]    = 0x6E402400,  // FCMGE .8H
    [GT]    = 0x6EC02400,  // FCMGT .8H
    [FMA]   = 0x4E400C00,  // FMLA  .8H
};
static uint32_t enc_three_reg(uint32_t base, int rd, int rn, int rm) {
    return base
         | ((uint32_t)(rm & 0x1F) << 16)
         | ((uint32_t)(rn & 0x1F) <<  5)
         | ((uint32_t)(rd & 0x1F)      );
}
static uint32_t enc_two_reg(uint32_t base, int rd, int rn) {
    return enc_three_reg(base, rd, rn, 0);
}

static uint32_t enc_LDP_Q(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset >> 4;
    return 0xAD400000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}
static uint32_t enc_STP_Q(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset >> 4;
    return 0xAD000000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}

static uint32_t enc_LDR_Q(int rt, int rn, int offset) {
    int const imm12 = offset >> 4;
    return 0x3DC00000
         | ((uint32_t)(imm12 & 0xFFF) << 10)
         | ((uint32_t)(rn    & 0x1F)  <<  5)
         | ((uint32_t)(rt    & 0x1F)       );
}
static uint32_t enc_STR_Q(int rt, int rn, int offset) {
    int const imm12 = offset >> 4;
    return 0x3D800000
         | ((uint32_t)(imm12 & 0xFFF) << 10)
         | ((uint32_t)(rn    & 0x1F)  <<  5)
         | ((uint32_t)(rt    & 0x1F)       );
}

// STP <Dt1>, <Dt2>, [SP, #-16]!  pre-indexed (push pair, scaled by 8)
static uint32_t enc_STP_D_pre(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset / 8;
    return 0x6D800000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}
// LDP <Dt1>, <Dt2>, [SP], #16    post-indexed (pop pair)
static uint32_t enc_LDP_D_post(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset / 8;
    return 0x6CC00000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}

#define ENC_PUSH_FP_LR 0xA9BF7BFD  // STP fp, lr, [SP, #-16]!
#define ENC_POP_FP_LR  0xA8C17BFD  // LDP fp, lr, [SP], #16
#define ENC_RET        0xD65F03C0

static uint32_t enc_SUB_SP(int imm12) {
    return 0xD10003FF | ((uint32_t)(imm12 & 0xFFF) << 10);
}
static uint32_t enc_ADD_SP(int imm12) {
    return 0x910003FF | ((uint32_t)(imm12 & 0xFFF) << 10);
}
static uint32_t enc_BL(int imm26) {
    return 0x94000000 | ((uint32_t)imm26 & 0x3FFFFFF);
}
static uint32_t enc_BLR(int rn) {
    return 0xD63F0000 | ((uint32_t)(rn & 0x1F) << 5);
}
// ADRP <Xd>, #imm21*4096   (signed 21-bit, page-aligned)
static uint32_t enc_ADRP(int rd, int imm21) {
    uint32_t const u = (uint32_t)imm21 & 0x1FFFFF;
    return 0x90000000
         | ((u & 0x3u) << 29)
         | (((u >> 2) & 0x7FFFFu) << 5)
         | ((uint32_t)(rd & 0x1F));
}
static uint32_t enc_ADD_imm(int rd, int rn, int imm12) {
    return 0x91000000
         | ((uint32_t)(imm12 & 0xFFF) << 10)
         | ((uint32_t)(rn & 0x1F) << 5)
         | ((uint32_t)(rd & 0x1F));
}
// MOV{Z,K} <Wd>, #imm16, LSL #(hw*16)   (32-bit immediate move)
static uint32_t enc_MOVZ_W(int rd, uint32_t imm16, int hw) {
    return 0x52800000
         | ((uint32_t)(hw & 3) << 21)
         | ((imm16 & 0xFFFF) << 5)
         | ((uint32_t)(rd & 0x1F));
}
static uint32_t enc_MOVK_W(int rd, uint32_t imm16, int hw) {
    return 0x72800000
         | ((uint32_t)(hw & 3) << 21)
         | ((imm16 & 0xFFFF) << 5)
         | ((uint32_t)(rd & 0x1F));
}

// Number of D-pairs we must save: V8..V15 lower 64 bits are callee-saved per AAPCS64.
static int callee_saves_f8(int regs) {
    return regs <= 4 ? 0 :
           regs <= 8 ? regs - 4 : 4;
}

static int callee_saves_1q(int regs) {
    if (regs <= 8) { return 0; }
    int need = (regs < 16 ? regs : 16) - 8;
    return (need + 1) / 2;
}

static _Bool has_op(struct rbb const *bb, enum rbb_op op) {
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op) {
            return 1;
        }
    }
    return 0;
}

// Returns the offset of the kernel within buf (= trampoline_size).
static size_t emit_jit_f8(struct rbb const *bb, void *buf) {
    enum { X0=0, SP=31, X16=16 };
    uint32_t *const buf_words = buf;
    uint32_t *out = buf_words;

    int const save_pairs = callee_saves_f8(bb->regs);
    size_t const trampoline_size = 4 * (size_t)(2*save_pairs + bb->in + bb->out + 4);

    *out++ = ENC_PUSH_FP_LR;
    for (int k = 0; k < save_pairs; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->in; r++) {
        *out++ = enc_LDP_Q(2*r, 2*r+1, X0, 32*r);
    }
    {
        ptrdiff_t const bl_word     = out - buf_words;
        ptrdiff_t const target_word = (ptrdiff_t)trampoline_size / 4;
        *out++ = enc_BL((int)(target_word - bl_word));
    }
    for (int r = 0; r < bb->out; r++) {
        *out++ = enc_STP_Q(2*r, 2*r+1, X0, 32*r);
    }
    for (int k = save_pairs; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_POP_FP_LR;
    *out++ = ENC_RET;
    assert( (char*)out - (char*)buf == (ptrdiff_t)trampoline_size );

    _Bool const kernel_has_call = has_op(bb, CALL);
    if (kernel_has_call) {
        *out++ = ENC_PUSH_FP_LR;
    }
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
                *out++ = enc_three_reg(enc_op_f[ip->op], 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(enc_op_f[ip->op], 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_f[ip->op], 2*ip->d,   2*ip->x);
                *out++ = enc_two_reg(enc_op_f[ip->op], 2*ip->d+1, 2*ip->x+1);
                break;

            case FMA:
                // FMLA Vd, Vx, Vy : Vd += Vx * Vy.
                *out++ = enc_three_reg(enc_op_f[FMA], 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(enc_op_f[FMA], 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                break;

            case SEL:
                // BSL Vd, Vx, Vy : Vd = (Vd & Vx) | (~Vd & Vy)  (mask = Vd).
                *out++ = enc_three_reg(0x6E601C00, 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(0x6E601C00, 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                break;

            case IMM: {
                int const d0 = 2*ip->d, d1 = d0+1;
                union { float f; uint32_t u; } v = { ip->imm };
                *out++ = enc_MOVZ_W(X16,  v.u        & 0xFFFF, 0);
                *out++ = enc_MOVK_W(X16, (v.u >> 16) & 0xFFFF, 1);
                *out++ = enc_two_reg(0x4E040C00, d0, X16);  // DUP d0.4S, X16
                *out++ = enc_three_reg(enc_op_f[OR],d1,d0,d0);
            } break;

            case CALL: {
                struct rbb const *callee = ip->call;
                int const cregs = callee->regs;
                int const cin   = callee->in;
                int const cout  = callee->out;
                int const d     = ip->d;
                int const save_count = d < cregs ? d : cregs;

                // Save only the chunks above the call frame; chunks inside the
                // frame are inputs we consume / outputs we'll overwrite / scratch.
                if (save_count > 0) {
                    *out++ = enc_SUB_SP(32 * save_count);
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_STP_Q(2*r, 2*r+1, SP, 32*r);
                    }
                }
                // Shift inputs into V[0..2*cin-1] (no-ops when d==0; skip emission).
                for (int j = 0; j < cin && d > 0; j++) {
                    *out++ = enc_three_reg(enc_op_f[OR], 2*j,   2*(d+j),   2*(d+j));
                    *out++ = enc_three_reg(enc_op_f[OR], 2*j+1, 2*(d+j)+1, 2*(d+j)+1);
                }
                // Cross-buffer absolute call: ADRP+ADD+BLR via X16.
                {
                    intptr_t const pc        = (intptr_t)out;
                    intptr_t const tgt       = (intptr_t)callee->jit_kernel_f8;
                    intptr_t const adrp_page = pc  & ~(intptr_t)0xFFF;
                    intptr_t const tgt_page  = tgt & ~(intptr_t)0xFFF;
                    int const imm21 = (int)((tgt_page - adrp_page) >> 12);
                    int const imm12 = (int)(tgt & 0xFFFu);
                    *out++ = enc_ADRP   (X16, imm21);
                    *out++ = enc_ADD_imm(X16, X16, imm12);
                    *out++ = enc_BLR    (X16);
                }
                // Shift outputs out (reverse order safe; no-ops when d==0).
                for (int j = cout; j --> 0 && d > 0;) {
                    *out++ = enc_three_reg(enc_op_f[OR], 2*(d+j),   2*j,   2*j);
                    *out++ = enc_three_reg(enc_op_f[OR], 2*(d+j)+1, 2*j+1, 2*j+1);
                }
                if (save_count > 0) {
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_LDP_Q(2*r, 2*r+1, SP, 32*r);
                    }
                    *out++ = enc_ADD_SP(32 * save_count);
                }
            } break;
        }
    }

    if (kernel_has_call) {
        *out++ = ENC_POP_FP_LR;
    }
    *out++ = ENC_RET;

    assert( (char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_f8 );
    return trampoline_size;
}

static size_t emit_jit_h8(struct rbb const *bb, void *buf) {
    enum { X0=0, SP=31, X16=16 };
    uint32_t *const buf_words = buf;
    uint32_t *out = buf_words;

    int const saves = callee_saves_1q(bb->regs);
    size_t const trampoline_size = 4 * (size_t)(2*saves + bb->in + bb->out + 4);

    *out++ = ENC_PUSH_FP_LR;
    for (int k = 0; k < saves; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->in; r++) {
        *out++ = enc_LDR_Q(r, X0, 16*r);
    }
    {
        ptrdiff_t const bl_word     = out - buf_words;
        ptrdiff_t const target_word = (ptrdiff_t)trampoline_size / 4;
        *out++ = enc_BL((int)(target_word - bl_word));
    }
    for (int r = 0; r < bb->out; r++) {
        *out++ = enc_STR_Q(r, X0, 16*r);
    }
    for (int k = saves; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_POP_FP_LR;
    *out++ = ENC_RET;
    assert( (char*)out - (char*)buf == (ptrdiff_t)trampoline_size );

    _Bool const kernel_has_call = has_op(bb, CALL);
    if (kernel_has_call) {
        *out++ = ENC_PUSH_FP_LR;
    }
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
                *out++ = enc_three_reg(enc_op_h[ip->op], ip->d, ip->x, ip->y);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_h[ip->op], ip->d, ip->x);
                break;

            case FMA:
                *out++ = enc_three_reg(enc_op_h[FMA], ip->d, ip->x, ip->y);
                break;

            case SEL:
                *out++ = enc_three_reg(0x6E601C00, ip->d, ip->x, ip->y);
                break;

            case IMM: {
                union { _Float16 h; uint16_t u; } v = { (_Float16)ip->imm };
                *out++ = enc_MOVZ_W(X16, v.u, 0);
                *out++ = enc_two_reg(0x4E020C00, ip->d, X16);  // DUP Vd.8H, W16
            } break;

            case CALL: {
                struct rbb const *callee = ip->call;
                int const cregs = callee->regs;
                int const cin   = callee->in;
                int const cout  = callee->out;
                int const d     = ip->d;
                int const save_count = d < cregs ? d : cregs;

                if (save_count > 0) {
                    *out++ = enc_SUB_SP(16 * save_count);
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_STR_Q(r, SP, 16*r);
                    }
                }
                for (int j = 0; j < cin && d > 0; j++) {
                    *out++ = enc_three_reg(enc_op_h[OR], j, d+j, d+j);
                }
                {
                    intptr_t const pc        = (intptr_t)out;
                    intptr_t const tgt       = (intptr_t)callee->jit_kernel_h8;
                    intptr_t const adrp_page = pc  & ~(intptr_t)0xFFF;
                    intptr_t const tgt_page  = tgt & ~(intptr_t)0xFFF;
                    int const imm21 = (int)((tgt_page - adrp_page) >> 12);
                    int const imm12 = (int)(tgt & 0xFFFu);
                    *out++ = enc_ADRP   (X16, imm21);
                    *out++ = enc_ADD_imm(X16, X16, imm12);
                    *out++ = enc_BLR    (X16);
                }
                for (int j = cout; j --> 0 && d > 0;) {
                    *out++ = enc_three_reg(enc_op_h[OR], d+j, j, j);
                }
                if (save_count > 0) {
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_LDR_Q(r, SP, 16*r);
                    }
                    *out++ = enc_ADD_SP(16 * save_count);
                }
            } break;
        }
    }

    if (kernel_has_call) {
        *out++ = ENC_POP_FP_LR;
    }
    *out++ = ENC_RET;

    assert( (char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_h8 );
    return trampoline_size;
}

static size_t emit_jit_f4(struct rbb const *bb, void *buf) {
    enum { X0=0, SP=31, X16=16 };
    uint32_t *const buf_words = buf;
    uint32_t *out = buf_words;

    int const saves = callee_saves_1q(bb->regs);
    size_t const trampoline_size = 4 * (size_t)(2*saves + 2*bb->in + 2*bb->out + 5);

    *out++ = ENC_PUSH_FP_LR;
    for (int k = 0; k < saves; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->in; r++) {
        *out++ = enc_LDR_Q(r, X0, 32*r);
    }
    {
        ptrdiff_t const bl_word     = out - buf_words;
        ptrdiff_t const target_word = (ptrdiff_t)trampoline_size / 4;
        *out++ = enc_BL((int)(target_word - bl_word));
    }
    for (int r = 0; r < bb->out; r++) {
        *out++ = enc_STR_Q(r, X0, 32*r);
    }
    for (int r = 0; r < bb->in; r++) {
        *out++ = enc_LDR_Q(r, X0, 32*r + 16);
    }
    {
        ptrdiff_t const bl_word     = out - buf_words;
        ptrdiff_t const target_word = (ptrdiff_t)trampoline_size / 4;
        *out++ = enc_BL((int)(target_word - bl_word));
    }
    for (int r = 0; r < bb->out; r++) {
        *out++ = enc_STR_Q(r, X0, 32*r + 16);
    }
    for (int k = saves; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_POP_FP_LR;
    *out++ = ENC_RET;
    assert( (char*)out - (char*)buf == (ptrdiff_t)trampoline_size );

    _Bool const kernel_has_call = has_op(bb, CALL);
    if (kernel_has_call) {
        *out++ = ENC_PUSH_FP_LR;
    }
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
                *out++ = enc_three_reg(enc_op_f[ip->op], ip->d, ip->x, ip->y);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_f[ip->op], ip->d, ip->x);
                break;

            case FMA:
                *out++ = enc_three_reg(enc_op_f[FMA], ip->d, ip->x, ip->y);
                break;

            case SEL:
                *out++ = enc_three_reg(0x6E601C00, ip->d, ip->x, ip->y);
                break;

            case IMM: {
                union { float f; uint32_t u; } v = { ip->imm };
                *out++ = enc_MOVZ_W(X16,  v.u        & 0xFFFF, 0);
                *out++ = enc_MOVK_W(X16, (v.u >> 16) & 0xFFFF, 1);
                *out++ = enc_two_reg(0x4E040C00, ip->d, X16);
            } break;

            case CALL: {
                struct rbb const *callee = ip->call;
                int const cregs = callee->regs;
                int const cin   = callee->in;
                int const cout  = callee->out;
                int const d     = ip->d;
                int const save_count = d < cregs ? d : cregs;

                if (save_count > 0) {
                    *out++ = enc_SUB_SP(16 * save_count);
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_STR_Q(r, SP, 16*r);
                    }
                }
                for (int j = 0; j < cin && d > 0; j++) {
                    *out++ = enc_three_reg(enc_op_f[OR], j, d+j, d+j);
                }
                {
                    intptr_t const pc        = (intptr_t)out;
                    intptr_t const tgt       = (intptr_t)callee->jit_kernel_f4;
                    intptr_t const adrp_page = pc  & ~(intptr_t)0xFFF;
                    intptr_t const tgt_page  = tgt & ~(intptr_t)0xFFF;
                    int const imm21 = (int)((tgt_page - adrp_page) >> 12);
                    int const imm12 = (int)(tgt & 0xFFFu);
                    *out++ = enc_ADRP   (X16, imm21);
                    *out++ = enc_ADD_imm(X16, X16, imm12);
                    *out++ = enc_BLR    (X16);
                }
                for (int j = cout; j --> 0 && d > 0;) {
                    *out++ = enc_three_reg(enc_op_f[OR], d+j, j, j);
                }
                if (save_count > 0) {
                    for (int r = 0; r < save_count; r++) {
                        *out++ = enc_LDR_Q(r, SP, 16*r);
                    }
                    *out++ = enc_ADD_SP(16 * save_count);
                }
            } break;
        }
    }

    if (kernel_has_call) {
        *out++ = ENC_POP_FP_LR;
    }
    *out++ = ENC_RET;

    assert( (char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_f4 );
    return trampoline_size;
}

struct rbb* rbb(struct rbb_inst const inst[], int insts) {
    size_t const inst_size = (size_t)insts * sizeof *inst;

    struct rbb *rbb = calloc(1, sizeof *rbb + inst_size);

    int max = -1;
    while (insts --> 0) {
        if (inst->op == CALL) {
            int const top = inst->d + inst->call->regs - 1;
            max = top > max ? top : max;
        } else {
            max = inst->d > max ? inst->d : max;
            for (uint8_t const *arg = &inst->x, *end = arg+arity(inst->op); arg != end; arg++) {
                max = *arg > max ? *arg : max;
            }
        }
        rbb->inst[rbb->insts++] = *inst++;
    }
    int const regs = max+1;
    rbb->regs = regs;

    struct {
        _Bool input, written, output;
    } *meta = calloc((size_t)regs, sizeof *meta);

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        if (ip->op == CALL) {
            for (int in = 0; in < ip->call->in; in++) {
                int const r = ip->d + in;
                if (!meta[r].written) {
                    meta[r].input = 1;
                }
                meta[r].output = 0;
            }
            for (int out = 0; out < ip->call->out; out++) {
                int const r = ip->d + out;
                meta[r].written = 1;
                meta[r].output  = 1;
            }
        } else {
            for (uint8_t const *arg = &ip->x, *end = arg+arity(ip->op); arg != end; arg++) {
                if (!meta[*arg].written) {
                    meta[*arg].input = 1;
                }
                meta[*arg].output = 0;
            }
            int d = ip->d;
            meta[d].written = 1;
            meta[d].output  = 1;
        }
    }

    for (int r = 0; r < regs; r++) {
        if (meta[r].input)  { rbb->in++; }
        if (meta[r].output) { rbb->out++; }
    }

    free(meta);

    if (rbb->regs <= 16) {
        int const save_pairs = callee_saves_f8(rbb->regs);
        size_t const trampoline = 4 * (size_t)(2*save_pairs + rbb->in + rbb->out + 4);
        size_t       body       = 0;
        size_t const x30_frame  = has_op(rbb, CALL) ? 8 : 0;
        for (int i = 0; i < rbb->insts; i++) {
            struct rbb_inst const *ip = rbb->inst + i;
            if (ip->op == CALL && ip->call->jit_kernel_f8 == NULL) {
                body = 0;
                break;
            }
            body += jit_inst_size_f8(ip);
        }
        if (rbb->insts == 0 || body > 0) {
            rbb->jit_size_f8 = trampoline + x30_frame + body + 4;
        }
    }

    if (rbb->jit_size_f8 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_f8,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            size_t const kernel_offset = emit_jit_f8(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_f8, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_f8);
                rbb->jit_trampoline_f8 = buf;
                rbb->jit_kernel_f8     = (char*)buf + kernel_offset;
            } else {
                munmap(buf, rbb->jit_size_f8);
            }
        }
    }

    if (rbb->regs <= 32) {
        int const saves = callee_saves_1q(rbb->regs);
        size_t const trampoline = 4 * (size_t)(2*saves + 2*rbb->in + 2*rbb->out + 5);
        size_t       body       = 0;
        size_t const x30_frame  = has_op(rbb, CALL) ? 8 : 0;
        for (int i = 0; i < rbb->insts; i++) {
            struct rbb_inst const *ip = rbb->inst + i;
            if (ip->op == CALL && ip->call->jit_kernel_f4 == NULL) {
                body = 0;
                break;
            }
            body += jit_inst_size_f4(ip);
        }
        if (rbb->insts == 0 || body > 0) {
            rbb->jit_size_f4 = trampoline + x30_frame + body + 4;
        }
    }

    if (rbb->jit_size_f4 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_f4,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            size_t const kernel_offset = emit_jit_f4(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_f4, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_f4);
                rbb->jit_trampoline_f4 = buf;
                rbb->jit_kernel_f4     = (char*)buf + kernel_offset;
            } else {
                munmap(buf, rbb->jit_size_f4);
            }
        }
    }

    if (rbb->regs <= 32) {
        int const saves = callee_saves_1q(rbb->regs);
        size_t const trampoline = 4 * (size_t)(2*saves + rbb->in + rbb->out + 4);
        size_t       body       = 0;
        size_t const x30_frame  = has_op(rbb, CALL) ? 8 : 0;
        for (int i = 0; i < rbb->insts; i++) {
            struct rbb_inst const *ip = rbb->inst + i;
            if (ip->op == CALL && ip->call->jit_kernel_h8 == NULL) {
                body = 0;
                break;
            }
            body += jit_inst_size_h8(ip);
        }
        if (rbb->insts == 0 || body > 0) {
            rbb->jit_size_h8 = trampoline + x30_frame + body + 4;
        }
    }

    if (rbb->jit_size_h8 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_h8,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            size_t const kernel_offset = emit_jit_h8(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_h8, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_h8);
                rbb->jit_trampoline_h8 = buf;
                rbb->jit_kernel_h8     = (char*)buf + kernel_offset;
            } else {
                munmap(buf, rbb->jit_size_h8);
            }
        }
    }

    return rbb;
}

void rbb_free(struct rbb *bb) {
    if (bb->jit_trampoline_f8) { munmap(bb->jit_trampoline_f8, bb->jit_size_f8); }
    if (bb->jit_trampoline_f4) { munmap(bb->jit_trampoline_f4, bb->jit_size_f4); }
    if (bb->jit_trampoline_h8) { munmap(bb->jit_trampoline_h8, bb->jit_size_h8); }
    free(bb);
}

struct rbb_meta rbb_meta(struct rbb const *bb) {
    return (struct rbb_meta){
        .inputs    = bb->in,
        .outputs   = bb->out,
        .registers = bb->regs,
        .jit_f     = bb->jit_trampoline_f8 != NULL || bb->jit_trampoline_f4 != NULL,
        .jit_h     = bb->jit_trampoline_h8 != NULL,
    };
}

void rbb_eval_f(struct rbb const *rbb, v8f reg[]) {
    if (rbb->jit_trampoline_f8) {
        void (*fn)(v8f*);
        memcpy(&fn, &rbb->jit_trampoline_f8, sizeof fn);
        fn(reg);
        return;
    }
    if (rbb->jit_trampoline_f4) {
        void (*fn)(v8f*);
        memcpy(&fn, &rbb->jit_trampoline_f4, sizeof fn);
        fn(reg);
        return;
    }

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        v8f d = {0};
        switch (ip->op) {
            case IMM:   d = ip->imm; break;

            case NEG:   d = -(reg[ip->x]); break;
            case ABS:   d = __builtin_elementwise_abs  (reg[ip->x]); break;
            case SQRT:  d = __builtin_elementwise_sqrt (reg[ip->x]); break;
            case FLOOR: d = __builtin_elementwise_floor(reg[ip->x]); break;
            case CEIL:  d = __builtin_elementwise_ceil (reg[ip->x]); break;
            case TRUNC: d = __builtin_elementwise_trunc(reg[ip->x]); break;
            case ROUND: d = __builtin_elementwise_round(reg[ip->x]); break;

            case ADD:   d = reg[ip->x] + reg[ip->y]; break;
            case SUB:   d = reg[ip->x] - reg[ip->y]; break;
            case MUL:   d = reg[ip->x] * reg[ip->y]; break;
            case DIV:   d = reg[ip->x] / reg[ip->y]; break;
            case MIN:   d = __builtin_elementwise_min(reg[ip->x], reg[ip->y]); break;
            case MAX:   d = __builtin_elementwise_max(reg[ip->x], reg[ip->y]); break;
            case FMA:   d = __builtin_elementwise_fma(reg[ip->x], reg[ip->y], reg[ip->d]); break;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wfloat-equal"
            case EQ:    d = (v8f)(reg[ip->x] == reg[ip->y]); break;
            case GT:    d = (v8f)(reg[ip->x] >  reg[ip->y]); break;
            case GE:    d = (v8f)(reg[ip->x] >= reg[ip->y]); break;
        #pragma GCC diagnostic pop

            case AND:   d = (v8f)( (v8i)reg[ip->x] & (v8i)reg[ip->y] ); break;
            case OR:    d = (v8f)( (v8i)reg[ip->x] | (v8i)reg[ip->y] ); break;
            case XOR:   d = (v8f)( (v8i)reg[ip->x] ^ (v8i)reg[ip->y] ); break;
            case NOT:   d = (v8f)(~(v8i)reg[ip->x]); break;
            case SEL:   d = (v8f)( ( (v8i)reg[ip->d] & (v8i)reg[ip->x])
                                 | (~(v8i)reg[ip->d] & (v8i)reg[ip->y]) ); break;

            case CALL: rbb_eval_f(ip->call, reg + ip->d); continue;
        }
        reg[ip->d] = d;
    }
}

void rbb_eval_h(struct rbb const *rbb, v8h reg[]) {
    if (rbb->jit_trampoline_h8) {
        void (*fn)(v8h*);
        memcpy(&fn, &rbb->jit_trampoline_h8, sizeof fn);
        fn(reg);
        return;
    }

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        v8h d = {0};
        switch (ip->op) {
            case IMM:   d = (_Float16)ip->imm; break;

            case NEG:   d = -(reg[ip->x]); break;
            case ABS:   d = __builtin_elementwise_abs  (reg[ip->x]); break;
            case SQRT:  d = __builtin_elementwise_sqrt (reg[ip->x]); break;
            case FLOOR: d = __builtin_elementwise_floor(reg[ip->x]); break;
            case CEIL:  d = __builtin_elementwise_ceil (reg[ip->x]); break;
            case TRUNC: d = __builtin_elementwise_trunc(reg[ip->x]); break;
            case ROUND: d = __builtin_elementwise_round(reg[ip->x]); break;

            case ADD:   d = reg[ip->x] + reg[ip->y]; break;
            case SUB:   d = reg[ip->x] - reg[ip->y]; break;
            case MUL:   d = reg[ip->x] * reg[ip->y]; break;
            case DIV:   d = reg[ip->x] / reg[ip->y]; break;
            case MIN:   d = __builtin_elementwise_min(reg[ip->x], reg[ip->y]); break;
            case MAX:   d = __builtin_elementwise_max(reg[ip->x], reg[ip->y]); break;
            case FMA:   d = __builtin_elementwise_fma(reg[ip->x], reg[ip->y], reg[ip->d]); break;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wfloat-equal"
            case EQ:    d = (v8h)(reg[ip->x] == reg[ip->y]); break;
            case GT:    d = (v8h)(reg[ip->x] >  reg[ip->y]); break;
            case GE:    d = (v8h)(reg[ip->x] >= reg[ip->y]); break;
        #pragma GCC diagnostic pop

            case AND:   d = (v8h)( (v8s)reg[ip->x] & (v8s)reg[ip->y] ); break;
            case OR:    d = (v8h)( (v8s)reg[ip->x] | (v8s)reg[ip->y] ); break;
            case XOR:   d = (v8h)( (v8s)reg[ip->x] ^ (v8s)reg[ip->y] ); break;
            case NOT:   d = (v8h)(~(v8s)reg[ip->x]); break;
            case SEL:   d = (v8h)( ( (v8s)reg[ip->d] & (v8s)reg[ip->x])
                                 | (~(v8s)reg[ip->d] & (v8s)reg[ip->y]) ); break;

            case CALL: rbb_eval_h(ip->call, reg + ip->d); continue;
        }
        reg[ip->d] = d;
    }
}
