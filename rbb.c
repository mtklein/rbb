#include "rbb.h"
#include <stdlib.h>

typedef int v8i __attribute__((ext_vector_type(8)));

struct rbb {
    int             insts,in,out,regs;
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

struct rbb* rbb(struct rbb_inst const inst[], int insts) {
    size_t const inst_size = (size_t)insts * sizeof *inst;

    struct rbb *rbb = calloc(1, sizeof *rbb + inst_size);

    int max = 0;
    while (insts --> 0) {
        max = inst->d > max ? inst->d : max;
        max = inst->x > max ? inst->x : max;
        max = inst->y > max ? inst->y : max;
        max = inst->z > max ? inst->z : max;

        rbb->inst[rbb->insts++] = *inst++;
    }
    int const regs = max+1;
    rbb->regs = regs;  // TODO: add in max of regs from any CALLs

    struct {
        _Bool input, written, output;
    } *meta = calloc((size_t)regs, sizeof *meta);

    for (int i = 0; i < rbb->insts; i++) {
        struct rbb_inst const *ip = rbb->inst + i;
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

    for (int r = 0; r < regs; r++) {
        if (meta[r].input)  { rbb->in++; }
        if (meta[r].output) { rbb->out++; }
    }

    free(meta);
    return rbb;
}

void rbb_free(struct rbb *rbb) {
    free(rbb);
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

            case CALL: break;  // TODO
        }
        reg[ip->d] = d;
    }
}
