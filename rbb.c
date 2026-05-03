#include "rbb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef int v8i __attribute__((ext_vector_type(8)));

#define MAX_JIT_REGS 16  // 32 NEON regs / 2 (v8f = 256 bits = 2 Q regs)

struct rbb {
    int             insts,in,out,regs;
    void           *jit_trampoline;  // trampoline void (*)(v8f*) called from rbb_eval()
    void           *jit_kernel;      // kernel entry BL'd from inner CALL ops
    size_t          jit_size;        // total mmap'd size, trampoline + kernel
    struct rbb_inst inst[];
};

static uint8_t const arity[] = {
    [NEG]=1, [ABS]=1, [SQRT]=1, [FLOOR]=1, [CEIL]=1, [TRUNC]=1, [ROUND]=1,
    [ADD]=2, [SUB]=2, [MUL]=2, [DIV]=2, [MIN]=2, [MAX]=2, [FMA]=3,
    [EQ]=2,  [LT]=2,  [LE]=2,
    [AND]=2, [OR]=2,  [XOR]=2, [NOT]=1, [SEL]=3,
};

static size_t jit_inst_size(struct rbb_inst const *ip) {
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
    static uint8_t const op_size[] = {
        [IMM]=16,
        [NEG]=8, [ABS]=8, [SQRT]=8, [FLOOR]=8, [CEIL]=8, [TRUNC]=8, [ROUND]=8,
        [ADD]=8, [SUB]=8, [MUL]=8, [DIV]=8, [MIN]=8, [MAX]=8, [FMA]=8,
        [EQ]=8,  [LT]=8,  [LE]=8,
        [AND]=8, [OR]=8,  [XOR]=8, [NOT]=8, [SEL]=8,
    };
    return op_size[ip->op];
}

static uint32_t const enc_op[] = {
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
    [LE]    = 0x6E20E400,  // FCMGE .4S (use with operands swapped: x<=y == y>=x)
    [LT]    = 0x6EA0E400,  // FCMGT .4S (use with operands swapped: x<y  == y>x)
    [FMA]   = 0x4E20CC00,  // FMLA  .4S
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
static int callee_saved_pairs(int regs) {
    return regs <= 4 ? 0 :
           regs <= 8 ? regs - 4 : 4;
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
static size_t emit_jit(struct rbb const *bb, void *buf) {
    enum { X0=0, SP=31, X16=16 };
    uint32_t *const buf_words = buf;
    uint32_t *out = buf_words;

    int const save_pairs = callee_saved_pairs(bb->regs);
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
            case EQ:
                *out++ = enc_three_reg(enc_op[ip->op], 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(enc_op[ip->op], 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                break;

            case LT: case LE:
                *out++ = enc_three_reg(enc_op[ip->op], 2*ip->d,   2*ip->y,   2*ip->x);
                *out++ = enc_three_reg(enc_op[ip->op], 2*ip->d+1, 2*ip->y+1, 2*ip->x+1);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op[ip->op], 2*ip->d,   2*ip->x);
                *out++ = enc_two_reg(enc_op[ip->op], 2*ip->d+1, 2*ip->x+1);
                break;

            case FMA:
                // FMLA Vd, Vx, Vy : Vd += Vx * Vy.
                *out++ = enc_three_reg(enc_op[FMA], 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(enc_op[FMA], 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
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
                *out++ = enc_three_reg(enc_op[OR],d1,d0,d0);
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
                    *out++ = enc_three_reg(enc_op[OR], 2*j,   2*(d+j),   2*(d+j));
                    *out++ = enc_three_reg(enc_op[OR], 2*j+1, 2*(d+j)+1, 2*(d+j)+1);
                }
                // Cross-buffer absolute call: ADRP+ADD+BLR via X16.
                {
                    intptr_t const pc        = (intptr_t)out;
                    intptr_t const tgt       = (intptr_t)callee->jit_kernel;
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
                    *out++ = enc_three_reg(enc_op[OR], 2*(d+j),   2*j,   2*j);
                    *out++ = enc_three_reg(enc_op[OR], 2*(d+j)+1, 2*j+1, 2*j+1);
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

    assert( (char*)out - (char*)buf == (ptrdiff_t)bb->jit_size );
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
            for (uint8_t const *arg = &inst->x, *end = arg+arity[inst->op]; arg != end; arg++) {
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
            for (uint8_t const *arg = &ip->x, *end = arg+arity[ip->op]; arg != end; arg++) {
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

    // jit_size:
    //   trampoline (PUSH FP/LR + save Ds + LDP inputs + BL kernel
    //               + STP outputs + restore Ds + POP FP/LR + RET)
    // + kernel    ((PUSH/POP X30 if has_op(CALL)) + body + RET)
    if (rbb->regs <= MAX_JIT_REGS) {
        int const save_pairs = callee_saved_pairs(rbb->regs);
        size_t const trampoline = 4 * (size_t)(2*save_pairs + rbb->in + rbb->out + 4);
        size_t       body       = 0;
        size_t const x30_frame  = has_op(rbb, CALL) ? 8 : 0;
        for (int i = 0; i < rbb->insts; i++) {
            struct rbb_inst const *ip = rbb->inst + i;
            if (ip->op == CALL && ip->call->jit_kernel == NULL) {
                body = 0;
                break;
            }
            body += jit_inst_size(ip);
        }
        if (rbb->insts == 0 || body > 0) {
            rbb->jit_size = trampoline + x30_frame + body + 4;  // +4 for kernel RET
        }
    }

    if (rbb->jit_size > 0) {
        void *buf = mmap(NULL, rbb->jit_size,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            size_t const kernel_offset = emit_jit(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size);
                rbb->jit_trampoline = buf;
                rbb->jit_kernel     = (char*)buf + kernel_offset;
            } else {
                munmap(buf, rbb->jit_size);
            }
        }
    }

    return rbb;
}

void rbb_free(struct rbb *bb) {
    if (bb->jit_trampoline) {
        munmap(bb->jit_trampoline, bb->jit_size);
    }
    free(bb);
}

struct rbb_meta rbb_meta(struct rbb const *bb) {
    return (struct rbb_meta){
        .inputs    = bb->in,
        .outputs   = bb->out,
        .registers = bb->regs,
        .jit       = bb->jit_trampoline != NULL,
    };
}

void rbb_eval(struct rbb const *rbb, v8f reg[]) {
    if (rbb->jit_trampoline) {
        void (*fn)(v8f*);
        memcpy(&fn, &rbb->jit_trampoline, sizeof fn);
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
            case LT:    d = (v8f)(reg[ip->x] <  reg[ip->y]); break;
            case LE:    d = (v8f)(reg[ip->x] <= reg[ip->y]); break;
        #pragma GCC diagnostic pop

            case AND:   d = (v8f)( (v8i)reg[ip->x] & (v8i)reg[ip->y] ); break;
            case OR:    d = (v8f)( (v8i)reg[ip->x] | (v8i)reg[ip->y] ); break;
            case XOR:   d = (v8f)( (v8i)reg[ip->x] ^ (v8i)reg[ip->y] ); break;
            case NOT:   d = (v8f)(~(v8i)reg[ip->x]); break;
            case SEL:   d = (v8f)( ( (v8i)reg[ip->d] & (v8i)reg[ip->x])
                                 | (~(v8i)reg[ip->d] & (v8i)reg[ip->y]) ); break;

            case CALL: rbb_eval(ip->call, reg + ip->d); continue;
        }
        reg[ip->d] = d;
    }
}
