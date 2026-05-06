#include "rbb.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

struct rbb {
    size_t jit_size_f8, jit_size_f4, jit_size_h8;
    void       *jit_f8,     *jit_f4,     *jit_h8;

    int        :32, insts, regs, ptrs;
    struct rbb_inst inst[];
};

static int arity(enum rbb_op op) {
    return op < STORE ? 0 :
           op < ADD   ? 1 :
           op < FMA   ? 2 : 3;
}

enum { X0=0, X1=1, X9=9, X16=16, SP=31 };

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
// LDR <Qt>, [<Xn>, <Xm>]   (register offset, no shift)
static uint32_t enc_LDR_Q_reg(int rt, int rn, int rm) {
    return 0x3CE06800
         | ((uint32_t)(rm & 0x1F) << 16)
         | ((uint32_t)(rn & 0x1F) <<  5)
         | ((uint32_t)(rt & 0x1F)      );
}
// STR <Qt>, [<Xn>, <Xm>]
static uint32_t enc_STR_Q_reg(int rt, int rn, int rm) {
    return 0x3CA06800
         | ((uint32_t)(rm & 0x1F) << 16)
         | ((uint32_t)(rn & 0x1F) <<  5)
         | ((uint32_t)(rt & 0x1F)      );
}
// LDR <Xt>, [<Xn>, #imm]   (unsigned offset, scaled by 8)
static uint32_t enc_LDR_X(int rt, int rn, int offset) {
    int const imm12 = offset >> 3;
    return 0xF9400000
         | ((uint32_t)(imm12 & 0xFFF) << 10)
         | ((uint32_t)(rn    & 0x1F)  <<  5)
         | ((uint32_t)(rt    & 0x1F)       );
}

// STP <Dt1>, <Dt2>, [SP, #-16]!
static uint32_t enc_STP_D_pre(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset / 8;
    return 0x6D800000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}
// LDP <Dt1>, <Dt2>, [SP], #16
static uint32_t enc_LDP_D_post(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset / 8;
    return 0x6CC00000
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}

#define ENC_RET 0xD65F03C0

static uint32_t enc_MOVZ_X(int rd, uint32_t imm16, int hw) {
    return 0xD2800000
         | ((uint32_t)(hw & 3) << 21)
         | ((imm16 & 0xFFFF) << 5)
         | ((uint32_t)(rd & 0x1F));
}
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

static uint32_t const enc_op_f[] = {
    [ADD]   = 0x4E20D400,  // FADD .4S
    [SUB]   = 0x4EA0D400,  // FSUB .4S
    [MUL]   = 0x6E20DC00,  // FMUL .4S
    [DIV]   = 0x6E20FC00,  // FDIV .4S
    [MIN]   = 0x4EA0F400,  // FMIN .4S
    [MAX]   = 0x4E20F400,  // FMAX .4S
    [NEG]   = 0x6EA0F800,  // FNEG  .4S
    [ABS]   = 0x4EA0F800,  // FABS  .4S
    [SQRT]  = 0x6EA1F800,  // FSQRT .4S
    [FLOOR] = 0x4E219800,  // FRINTM .4S
    [CEIL]  = 0x4EA18800,  // FRINTP .4S
    [TRUNC] = 0x4EA19800,  // FRINTZ .4S
    [ROUND] = 0x6E218800,  // FRINTA .4S
    [AND]   = 0x4E201C00,  // AND .16B
    [OR]    = 0x4EA01C00,  // ORR .16B
    [XOR]   = 0x6E201C00,  // EOR .16B
    [NOT]   = 0x6E205800,  // NOT .16B
    [EQ]    = 0x4E20E400,  // FCMEQ .4S
    [GE]    = 0x6E20E400,  // FCMGE .4S
    [GT]    = 0x6EA0E400,  // FCMGT .4S
    [FMA]   = 0x4E20CC00,  // FMLA .4S
    [SEL]   = 0x6E601C00,  // BSL .16B
};
static uint32_t const enc_op_h[] = {
    [ADD]   = 0x4E401400,  // FADD .8H
    [SUB]   = 0x4EC01400,  // FSUB .8H
    [MUL]   = 0x6E401C00,  // FMUL .8H
    [DIV]   = 0x6E403C00,  // FDIV .8H
    [MIN]   = 0x4EC03400,  // FMIN .8H
    [MAX]   = 0x4E403400,  // FMAX .8H
    [NEG]   = 0x6EF8F800,  // FNEG  .8H
    [ABS]   = 0x4EF8F800,  // FABS  .8H
    [SQRT]  = 0x6EF9F800,  // FSQRT .8H
    [FLOOR] = 0x4E799800,  // FRINTM .8H
    [CEIL]  = 0x4EF98800,  // FRINTP .8H
    [TRUNC] = 0x4EF99800,  // FRINTZ .8H
    [ROUND] = 0x6E798800,  // FRINTA .8H
    [AND]   = 0x4E201C00,
    [OR]    = 0x4EA01C00,
    [XOR]   = 0x6E201C00,
    [NOT]   = 0x6E205800,
    [EQ]    = 0x4E402400,  // FCMEQ .8H
    [GE]    = 0x6E402400,  // FCMGE .8H
    [GT]    = 0x6EC02400,  // FCMGT .8H
    [FMA]   = 0x4E400C00,  // FMLA .8H
    [SEL]   = 0x6E601C00,  // BSL .16B
};

// Number of D-pairs we save: V8..V15 lower 64 bits are callee-saved per AAPCS64.
static int callee_saves_f8(int regs) {
    return regs <= 4 ? 0 :
           regs <= 8 ? regs - 4 : 4;
}
static int callee_saves_1q(int regs) {
    if (regs <= 8) { return 0; }
    int need = (regs < 16 ? regs : 16) - 8;
    return (need + 1) / 2;
}

static size_t body_size_f8(struct rbb_inst const *ip) {
    return ip->op == IMM ? 16 : 8;
}
static size_t body_size_f4(struct rbb_inst const *ip) {
    return ip->op == IMM   ? 12 :
           ip->op == LOAD  ?  8 :
           ip->op == STORE ?  8 : 4;
}
static size_t body_size_h8(struct rbb_inst const *ip) {
    return ip->op == IMM   ? 8 :
           ip->op == LOAD  ? 8 :
           ip->op == STORE ? 8 : 4;
}

static uint32_t* emit_body_f8(struct rbb const *bb, uint32_t *out) {
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
            case FMA: case SEL:
                *out++ = enc_three_reg(enc_op_f[ip->op], 2*ip->d,   2*ip->x,   2*ip->y);
                *out++ = enc_three_reg(enc_op_f[ip->op], 2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_f[ip->op], 2*ip->d,   2*ip->x);
                *out++ = enc_two_reg(enc_op_f[ip->op], 2*ip->d+1, 2*ip->x+1);
                break;

            case IMM: {
                int const d0 = 2*ip->d, d1 = d0+1;
                union { float f; uint32_t u; } v = { ip->imm };
                *out++ = enc_MOVZ_W(X16,  v.u        & 0xFFFF, 0);
                *out++ = enc_MOVK_W(X16, (v.u >> 16) & 0xFFFF, 1);
                *out++ = enc_two_reg(0x4E040C00, d0, X16);  // DUP Vd.4S, W16
                *out++ = enc_three_reg(enc_op_f[OR], d1, d0, d0);
            } break;

            case LOAD:
                *out++ = enc_LDR_X(X16, X1, 8*ip->x);
                *out++ = enc_LDP_Q(2*ip->d, 2*ip->d+1, X16, 0);
                break;
            case STORE:
                *out++ = enc_LDR_X(X16, X1, 8*ip->d);
                *out++ = enc_STP_Q(2*ip->x, 2*ip->x+1, X16, 0);
                break;
        }
    }
    return out;
}

static uint32_t* emit_body_f4(struct rbb const *bb, uint32_t *out) {
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
            case FMA: case SEL:
                *out++ = enc_three_reg(enc_op_f[ip->op], ip->d, ip->x, ip->y);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_f[ip->op], ip->d, ip->x);
                break;

            case IMM: {
                union { float f; uint32_t u; } v = { ip->imm };
                *out++ = enc_MOVZ_W(X16,  v.u        & 0xFFFF, 0);
                *out++ = enc_MOVK_W(X16, (v.u >> 16) & 0xFFFF, 1);
                *out++ = enc_two_reg(0x4E040C00, ip->d, X16);  // DUP Vd.4S, W16
            } break;

            case LOAD:
                *out++ = enc_LDR_X    (X16, X1, 8*ip->x);
                *out++ = enc_LDR_Q_reg(ip->d, X16, X9);
                break;
            case STORE:
                *out++ = enc_LDR_X    (X16, X1, 8*ip->d);
                *out++ = enc_STR_Q_reg(ip->x, X16, X9);
                break;
        }
    }
    return out;
}

static uint32_t* emit_body_h8(struct rbb const *bb, uint32_t *out) {
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
            case FMA: case SEL:
                *out++ = enc_three_reg(enc_op_h[ip->op], ip->d, ip->x, ip->y);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op_h[ip->op], ip->d, ip->x);
                break;

            case IMM: {
                union { _Float16 h; uint16_t u; } v = { (_Float16)ip->imm };
                *out++ = enc_MOVZ_W(X16, v.u, 0);
                *out++ = enc_two_reg(0x4E020C00, ip->d, X16);  // DUP Vd.8H, W16
            } break;

            case LOAD:
                *out++ = enc_LDR_X(X16, X1, 8*ip->x);
                *out++ = enc_LDR_Q(ip->d, X16, 0);
                break;
            case STORE:
                *out++ = enc_LDR_X(X16, X1, 8*ip->d);
                *out++ = enc_STR_Q(ip->x, X16, 0);
                break;
        }
    }
    return out;
}

static void emit_jit_f8(struct rbb const *bb, void *buf) {
    uint32_t *out = buf;
    int const save_pairs = callee_saves_f8(bb->regs);
    for (int k = 0; k < save_pairs; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_LDP_Q(2*r, 2*r+1, X0, 32*r);
    }
    out = emit_body_f8(bb, out);
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_STP_Q(2*r, 2*r+1, X0, 32*r);
    }
    for (int k = save_pairs; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_RET;
    __builtin_assume((char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_f8);
}

static void emit_jit_h8(struct rbb const *bb, void *buf) {
    uint32_t *out = buf;
    int const saves = callee_saves_1q(bb->regs);
    for (int k = 0; k < saves; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_LDR_Q(r, X0, 16*r);
    }
    out = emit_body_h8(bb, out);
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_STR_Q(r, X0, 16*r);
    }
    for (int k = saves; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_RET;
    __builtin_assume((char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_h8);
}

static void emit_jit_f4(struct rbb const *bb, void *buf) {
    uint32_t *out = buf;
    int const saves = callee_saves_1q(bb->regs);
    for (int k = 0; k < saves; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    { // lo half
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_LDR_Q(r, X0, 32*r);
        }
        *out++ = enc_MOVZ_X(X9, 0, 0);
        out = emit_body_f4(bb, out);
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_STR_Q(r, X0, 32*r);
        }
    }
    { // hi half
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_LDR_Q(r, X0, 32*r + 16);
        }
        *out++ = enc_MOVZ_X(X9, 16, 0);
        out = emit_body_f4(bb, out);
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_STR_Q(r, X0, 32*r + 16);
        }
    }
    for (int k = saves; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_RET;
    __builtin_assume((char*)out - (char*)buf == (ptrdiff_t)bb->jit_size_f4);
}

struct rbb* rbb(struct rbb_inst const inst[], int insts) {
    size_t const inst_size = (size_t)insts * sizeof *inst;
    struct rbb *rbb = calloc(1, sizeof *rbb + inst_size);

    int max_reg = -1,
        max_ptr = -1;
    while (insts --> 0) {
        if (inst->op == STORE) { max_ptr = inst->d > max_ptr ? inst->d : max_ptr; }
        else                   { max_reg = inst->d > max_reg ? inst->d : max_reg; }

        if (inst->op == LOAD)  { max_ptr = inst->x > max_ptr ? inst->x : max_ptr; }
        else switch (arity(inst->op)) {
            case 3: max_reg = inst->d > max_reg ? inst->d : max_reg; __attribute__((fallthrough));
            case 2: max_reg = inst->y > max_reg ? inst->y : max_reg; __attribute__((fallthrough));
            case 1: max_reg = inst->x > max_reg ? inst->x : max_reg;
        }

        rbb->inst[rbb->insts++] = *inst++;
    }
    rbb->regs = max_reg + 1;
    rbb->ptrs = max_ptr + 1;

    if (rbb->regs <= 16) {
        rbb->jit_size_f8 = 4*(size_t)(2*callee_saves_f8(rbb->regs) + 2*rbb->regs + 1);
        for (int i = 0; i < rbb->insts; i++) {
            rbb->jit_size_f8 += body_size_f8(rbb->inst + i);
        }
    }
    if (rbb->jit_size_f8 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_f8,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            emit_jit_f8(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_f8, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_f8);
                rbb->jit_f8 = buf;
            } else {
                munmap(buf, rbb->jit_size_f8);
            }
        }
    }

    if (rbb->regs <= 32) {
        rbb->jit_size_f4 = 4*(size_t)(2*callee_saves_1q(rbb->regs) + 4*rbb->regs + 3);
        for (int i = 0; i < rbb->insts; i++) {
            rbb->jit_size_f4 += 2*body_size_f4(rbb->inst + i);
        }
    }
    if (rbb->jit_size_f4 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_f4,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            emit_jit_f4(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_f4, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_f4);
                rbb->jit_f4 = buf;
            } else {
                munmap(buf, rbb->jit_size_f4);
            }
        }
    }

    if (rbb->regs <= 32) {
        rbb->jit_size_h8 = 4*(size_t)(2*callee_saves_1q(rbb->regs) + 2*rbb->regs + 1);
        for (int i = 0; i < rbb->insts; i++) {
            rbb->jit_size_h8 += body_size_h8(rbb->inst + i);
        }
    }
    if (rbb->jit_size_h8 > 0) {
        void *buf = mmap(NULL, rbb->jit_size_h8,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            emit_jit_h8(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size_h8, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size_h8);
                rbb->jit_h8 = buf;
            } else {
                munmap(buf, rbb->jit_size_h8);
            }
        }
    }

    return rbb;
}

void rbb_free(struct rbb *bb) {
    if (bb->jit_f8) { munmap(bb->jit_f8, bb->jit_size_f8); }
    if (bb->jit_f4) { munmap(bb->jit_f4, bb->jit_size_f4); }
    if (bb->jit_h8) { munmap(bb->jit_h8, bb->jit_size_h8); }
    free(bb);
}

int rbb_regs(struct rbb const *bb) { return bb->regs; }
int rbb_ptrs(struct rbb const *bb) { return bb->ptrs; }

void rbb_eval_f(struct rbb const *rbb, v8f reg[], void *ptr[]) {
    if (rbb->jit_f8) {
        void (*fn)(v8f[], void*[]);
        memcpy(&fn, &rbb->jit_f8, sizeof fn);
        fn(reg, ptr);
        return;
    }
    if (rbb->jit_f4) {
        void (*fn)(v8f[], void*[]);
        memcpy(&fn, &rbb->jit_f4, sizeof fn);
        fn(reg, ptr);
        return;
    }

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        v8f d = {0};
        switch (ip->op) {
            case IMM:   d = ip->imm; break;
            case LOAD:  __builtin_memcpy(&d, ptr[ip->x], sizeof d); break;
            case STORE: __builtin_memcpy(ptr[ip->d], reg + ip->x, sizeof *reg); continue;

            case NEG:   d = -reg[ip->x]; break;
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
        #pragma GCC diagnostic pop
            case GT:    d = (v8f)(reg[ip->x] >  reg[ip->y]); break;
            case GE:    d = (v8f)(reg[ip->x] >= reg[ip->y]); break;

            case AND:   d = (v8f)( (v8i)reg[ip->x] & (v8i)reg[ip->y] ); break;
            case OR:    d = (v8f)( (v8i)reg[ip->x] | (v8i)reg[ip->y] ); break;
            case XOR:   d = (v8f)( (v8i)reg[ip->x] ^ (v8i)reg[ip->y] ); break;
            case NOT:   d = (v8f)(~(v8i)reg[ip->x]); break;
            case SEL:   d = (v8f)( ( (v8i)reg[ip->d] & (v8i)reg[ip->x])
                                 | (~(v8i)reg[ip->d] & (v8i)reg[ip->y]) ); break;
        }
        reg[ip->d] = d;
    }
}

void rbb_eval_h(struct rbb const *rbb, v8h reg[], void *ptr[]) {
    if (rbb->jit_h8) {
        void (*fn)(v8h[], void*[]);
        memcpy(&fn, &rbb->jit_h8, sizeof fn);
        fn(reg, ptr);
        return;
    }

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        v8h d = {0};
        switch (ip->op) {
            case IMM:   d = (_Float16)ip->imm; break;
            case LOAD:  __builtin_memcpy(&d, ptr[ip->x], sizeof d); break;
            case STORE: __builtin_memcpy(ptr[ip->d], reg + ip->x, sizeof *reg); continue;

            case NEG:   d = -reg[ip->x]; break;
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
        #pragma GCC diagnostic pop
            case GT:    d = (v8h)(reg[ip->x] >  reg[ip->y]); break;
            case GE:    d = (v8h)(reg[ip->x] >= reg[ip->y]); break;

            case AND:   d = (v8h)( (v8s)reg[ip->x] & (v8s)reg[ip->y] ); break;
            case OR:    d = (v8h)( (v8s)reg[ip->x] | (v8s)reg[ip->y] ); break;
            case XOR:   d = (v8h)( (v8s)reg[ip->x] ^ (v8s)reg[ip->y] ); break;
            case NOT:   d = (v8h)(~(v8s)reg[ip->x]); break;
            case SEL:   d = (v8h)( ( (v8s)reg[ip->d] & (v8s)reg[ip->x])
                                 | (~(v8s)reg[ip->d] & (v8s)reg[ip->y]) ); break;
        }
        reg[ip->d] = d;
    }
}
