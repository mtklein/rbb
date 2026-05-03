#include "rbb.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef int v8i __attribute__((ext_vector_type(8)));

#define MAX_JIT_REGS 16  // 32 NEON regs / 2 (v8f = 256 bits = 2 Q regs)

struct rbb {
    int             insts,in,out,regs;
    size_t          jit_size;
    struct rbb_inst inst[];
};

static int const arity[] = {
    [IMM]=0,
    [NEG]=1, [ABS]=1, [SQRT]=1, [FLOOR]=1, [CEIL]=1, [TRUNC]=1, [ROUND]=1,
    [ADD]=2, [SUB]=2, [MUL]=2, [DIV]=2, [MIN]=2, [MAX]=2, [FMA]=3,
    [EQ]=2,  [NE]=2,  [LT]=2,  [LE]=2,
    [AND]=2, [OR]=2,  [XOR]=2, [NOT]=1, [SEL]=3,
    [CALL]  = 0,  // TODO
};

// Bytes emitted per op in the JIT.  0 means "not yet supported".
static size_t const jit_op_size[] = {
    [IMM]=0,
    [NEG]=0, [ABS]=0, [SQRT]=0, [FLOOR]=0, [CEIL]=0, [TRUNC]=0, [ROUND]=0,
    [ADD]=8, [SUB]=0, [MUL]=0, [DIV]=0, [MIN]=0, [MAX]=0, [FMA]=0,
    [EQ]=0,  [NE]=0,  [LT]=0,  [LE]=0,
    [AND]=0, [OR]=0,  [XOR]=0, [NOT]=0, [SEL]=0,
    [CALL]=0,
};
static _Bool can_jit(enum rbb_op op) {
    return jit_op_size[op] > 0;
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
            for (short const *arg = &inst->x, *end = arg+arity[inst->op]; arg != end; arg++) {
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
            for (short const *arg = &ip->x, *end = arg+arity[ip->op]; arg != end; arg++) {
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

    // jit_size: prologue (LDP per reg) + body (per-op) + epilogue (STP per reg) + RET
    if (rbb->regs <= MAX_JIT_REGS) {
        rbb->jit_size += (size_t)(8 * rbb->regs) + 4;
        for (int i = 0; i < rbb->insts; i++) {
            if (can_jit(rbb->inst[i].op)) {
                rbb->jit_size += jit_op_size[rbb->inst[i].op];
            } else {
                rbb->jit_size = 0;
                break;
            }
        }
    }

    return rbb;
}

void rbb_free(struct rbb *rbb) {
    free(rbb);
}

struct rbb_meta rbb_meta(struct rbb const *bb) {
    return (struct rbb_meta){
        .inputs    = bb->in,
        .outputs   = bb->out,
        .registers = bb->regs,
        .jit_size  = bb->jit_size,
    };
}

void rbb_eval(struct rbb const *rbb, v8f reg[]) {
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
            case FMA:   d = __builtin_elementwise_fma(reg[ip->x], reg[ip->y], reg[ip->z]); break;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wfloat-equal"
            case EQ:    d = (v8f)(reg[ip->x] == reg[ip->y]); break;
            case NE:    d = (v8f)(reg[ip->x] != reg[ip->y]); break;
            case LT:    d = (v8f)(reg[ip->x] <  reg[ip->y]); break;
            case LE:    d = (v8f)(reg[ip->x] <= reg[ip->y]); break;
        #pragma GCC diagnostic pop

            case AND:   d = (v8f)( (v8i)reg[ip->x] & (v8i)reg[ip->y] ); break;
            case OR:    d = (v8f)( (v8i)reg[ip->x] | (v8i)reg[ip->y] ); break;
            case XOR:   d = (v8f)( (v8i)reg[ip->x] ^ (v8i)reg[ip->y] ); break;
            case NOT:   d = (v8f)(~(v8i)reg[ip->x]); break;
            case SEL:   d = (v8f)( ( (v8i)reg[ip->x] & (v8i)reg[ip->y])
                                 | (~(v8i)reg[ip->x] & (v8i)reg[ip->z]) ); break;

            case CALL: rbb_eval(ip->call, reg + ip->d); continue;
        }
        reg[ip->d] = d;
    }
}

// AArch64 instruction encoders.  Rt/Rt2/Rn/Rd/Rm fit in 5 bits.
//
// LDP <Qt1>, <Qt2>, [<Xn>, #imm]   imm signed 7-bit, scaled by 16
// STP same with bit 22 (L) cleared
// FADD <Vd>.4S, <Vn>.4S, <Vm>.4S
// RET (default Xn=x30) = 0xD65F03C0

static uint32_t enc_LDP_Q(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset >> 4;
    return 0xAD400000u
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}
static uint32_t enc_STP_Q(int rt, int rt2, int rn, int offset) {
    int const imm7 = offset >> 4;
    return 0xAD000000u
         | ((uint32_t)(imm7 & 0x7F) << 15)
         | ((uint32_t)(rt2  & 0x1F) << 10)
         | ((uint32_t)(rn   & 0x1F) <<  5)
         | ((uint32_t)(rt   & 0x1F)      );
}
static uint32_t enc_FADD_4S(int rd, int rn, int rm) {
    return 0x4E20D400u
         | ((uint32_t)(rm & 0x1F) << 16)
         | ((uint32_t)(rn & 0x1F) <<  5)
         | ((uint32_t)(rd & 0x1F)      );
}

_Bool rbb_jit(struct rbb const *bb, void *buf) {
    uint32_t *out = buf;
    if (bb->jit_size) {
        // Prologue: load each rbb register from x0[#offset] into a Q pair.
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_LDP_Q(2*r, 2*r+1, /*x0*/0, 32*r);
        }

        for (int i = 0; i < bb->insts; i++) {
            struct rbb_inst const *ip = bb->inst + i;
            switch (ip->op) {
                case ADD:
                    *out++ = enc_FADD_4S(2*ip->d,   2*ip->x,   2*ip->y);
                    *out++ = enc_FADD_4S(2*ip->d+1, 2*ip->x+1, 2*ip->y+1);
                    break;

                case IMM:
                case NEG: case ABS: case SQRT: case FLOOR:
                case CEIL: case TRUNC: case ROUND:
                case SUB: case MUL: case DIV: case MIN: case MAX: case FMA:
                case EQ: case NE: case LT: case LE:
                case AND: case OR: case XOR: case NOT: case SEL:
                case CALL:
                    return 0;
            }
        }

        // Epilogue: store each rbb register back to x0[#offset], then return.
        for (int r = 0; r < bb->regs; r++) {
            *out++ = enc_STP_Q(2*r, 2*r+1, /*x0*/0, 32*r);
        }
        *out++ = 0xD65F03C0u;  // RET
        return 1;
    }
    assert( (char*)out - (char*)buf == (ptrdiff_t)bb->jit_size );
    return 0;


}
