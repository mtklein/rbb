#include "rbb.h"
#include <stdlib.h>
#include <string.h>

typedef int v8i __attribute__((ext_vector_type(8)));

struct rbb {
    int             insts;
    struct rbb_inst inst[];
};

struct rbb* rbb(struct rbb_inst const inst[], int insts) {
    size_t const inst_size = (size_t)insts * sizeof *inst;

    struct rbb *rbb = malloc(sizeof *rbb + inst_size);
    rbb->insts = insts;
    memcpy(rbb->inst, inst, inst_size);
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
        }
        reg[ip->d] = d;
    }
}
