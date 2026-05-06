#include "rbb.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

struct rbb {
    void           *jit;
    size_t          jit_size;
    int             insts, regs;
    struct rbb_inst inst[];
};

static int arity(enum rbb_op op) {
    return op < NEG ? 0 :
           op < ADD ? 1 :
           op < FMA ? 2 : 3;
}

enum { X0=0, X16=16, SP=31 };

static uint32_t enc_three_reg(uint32_t base, int rd, int rn, int rm) {
    return base
         | ((uint32_t)(rm & 0x1F) << 16)
         | ((uint32_t)(rn & 0x1F) <<  5)
         | ((uint32_t)(rd & 0x1F)      );
}
static uint32_t enc_two_reg(uint32_t base, int rd, int rn) {
    return enc_three_reg(base, rd, rn, 0);
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

static uint32_t enc_MOVZ_W(int rd, uint32_t imm16, int hw) {
    return 0x52800000
         | ((uint32_t)(hw & 3) << 21)
         | ((imm16 & 0xFFFF) << 5)
         | ((uint32_t)(rd & 0x1F));
}

static uint32_t const enc_op[] = {
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
    [AND]   = 0x4E201C00,  // AND .16B
    [OR]    = 0x4EA01C00,  // ORR .16B
    [XOR]   = 0x6E201C00,  // EOR .16B
    [NOT]   = 0x6E205800,  // NOT .16B
    [EQ]    = 0x4E402400,  // FCMEQ .8H
    [GE]    = 0x6E402400,  // FCMGE .8H
    [GT]    = 0x6EC02400,  // FCMGT .8H
    [FMA]   = 0x4E400C00,  // FMLA .8H
    [SEL]   = 0x6E601C00,  // BSL .16B
};

// Number of D-pairs we save: V8..V15 lower 64 bits are callee-saved per AAPCS64.
static int callee_saves(int regs) {
    if (regs <= 8) { return 0; }
    int need = (regs < 16 ? regs : 16) - 8;
    return (need + 1) / 2;
}

static size_t body_size(struct rbb_inst const *ip) {
    return ip->op == IMM ? 8 : 4;
}

static uint32_t* emit_body(struct rbb const *bb, uint32_t *out) {
    for (int i = 0; i < bb->insts; i++) {
        struct rbb_inst const *ip = bb->inst + i;
        switch (ip->op) {
            case ADD: case SUB: case MUL: case DIV: case MIN: case MAX:
            case AND: case OR:  case XOR:
            case EQ:  case GT:  case GE:
            case FMA: case SEL:
                *out++ = enc_three_reg(enc_op[ip->op], ip->d, ip->x, ip->y);
                break;

            case NEG: case ABS: case NOT:
            case SQRT: case FLOOR: case CEIL: case TRUNC: case ROUND:
                *out++ = enc_two_reg(enc_op[ip->op], ip->d, ip->x);
                break;

            case IMM: {
                union { _Float16 h; uint16_t u; } v = { ip->imm };
                *out++ = enc_MOVZ_W(X16, v.u, 0);
                *out++ = enc_two_reg(0x4E020C00, ip->d, X16);  // DUP Vd.8H, W16
            } break;
        }
    }
    return out;
}

static void emit_jit(struct rbb const *bb, void *buf) {
    uint32_t *out = buf;
    int const saves = callee_saves(bb->regs);
    for (int k = 0; k < saves; k++) {
        *out++ = enc_STP_D_pre(2*k+8, 2*k+9, SP, -16);
    }
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_LDR_Q(r, X0, 16*r);
    }
    out = emit_body(bb, out);
    for (int r = 0; r < bb->regs; r++) {
        *out++ = enc_STR_Q(r, X0, 16*r);
    }
    for (int k = saves; k --> 0;) {
        *out++ = enc_LDP_D_post(2*k+8, 2*k+9, SP, 16);
    }
    *out++ = ENC_RET;
    __builtin_assume((char*)out - (char*)buf == (ptrdiff_t)bb->jit_size);
}

struct rbb* rbb(struct rbb_inst const inst[], int insts) {
    size_t const inst_size = (size_t)insts * sizeof *inst;
    struct rbb *rbb = calloc(1, sizeof *rbb + inst_size);

    int max_reg = -1;
    while (insts --> 0) {
        max_reg = inst->d > max_reg ? inst->d : max_reg;
        switch (arity(inst->op)) {
            case 3: max_reg = inst->d > max_reg ? inst->d : max_reg; __attribute__((fallthrough));
            case 2: max_reg = inst->y > max_reg ? inst->y : max_reg; __attribute__((fallthrough));
            case 1: max_reg = inst->x > max_reg ? inst->x : max_reg;
        }
        rbb->inst[rbb->insts++] = *inst++;
    }
    rbb->regs = max_reg + 1;

    if (rbb->regs <= 32) {
        rbb->jit_size = 4*(size_t)(2*callee_saves(rbb->regs) + 2*rbb->regs + 1);
        for (int i = 0; i < rbb->insts; i++) {
            rbb->jit_size += body_size(rbb->inst + i);
        }
    }
    if (rbb->jit_size > 0) {
        void *buf = mmap(NULL, rbb->jit_size,
                         PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (buf != MAP_FAILED) {
            emit_jit(rbb, buf);
            if (0 == mprotect(buf, rbb->jit_size, PROT_READ|PROT_EXEC)) {
                __builtin___clear_cache(buf, (char*)buf + rbb->jit_size);
                rbb->jit = buf;
            } else {
                munmap(buf, rbb->jit_size);
            }
        }
    }

    return rbb;
}

void rbb_free(struct rbb *bb) {
    if (bb->jit) { munmap(bb->jit, bb->jit_size); }
    free(bb);
}

int rbb_regs(struct rbb const *bb) { return bb->regs; }

void rbb_eval(struct rbb const *rbb, v8h reg[]) {
    if (rbb->jit) {
        void (*fn)(v8h[]);
        memcpy(&fn, &rbb->jit, sizeof fn);
        fn(reg);
        return;
    }

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
        v8h d = {0};
        switch (ip->op) {
            case IMM:   d = ip->imm; break;

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
