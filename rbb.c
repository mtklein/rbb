#include "rbb.h"
#include <math.h>

enum {
    ABS=1, NEG, SQRT,
    ADD, SUB, MUL, DIV, FMA,
    EQ, NE, LT, LE, SEL,
    CALL,
};

typedef union {
    uint32_t bits;
    float    imm;
    struct {
        uint32_t op : 5,
                  x : 6,
                  y : 6,
                  z : 6,
                 hi : 9;
    };
} inst;

uint32_t rbb_imm(float imm) { return (inst){.imm=imm}.bits; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
uint32_t rbb_abs (int x) { return (inst){.op=ABS , .x=x, .hi=-1}.bits; }
uint32_t rbb_neg (int x) { return (inst){.op=NEG , .x=x, .hi=-1}.bits; }
uint32_t rbb_sqrt(int x) { return (inst){.op=SQRT, .x=x, .hi=-1}.bits; }

uint32_t rbb_add(int x, int y       ) { return (inst){.op=ADD, .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_sub(int x, int y       ) { return (inst){.op=SUB, .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_mul(int x, int y       ) { return (inst){.op=MUL, .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_div(int x, int y       ) { return (inst){.op=DIV, .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_fma(int x, int y, int z) { return (inst){.op=FMA, .x=x, .y=y, .z=z, .hi=-1}.bits; }

uint32_t rbb_eq (int x, int y       ) { return (inst){.op=EQ , .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_ne (int x, int y       ) { return (inst){.op=NE , .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_lt (int x, int y       ) { return (inst){.op=LT , .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_le (int x, int y       ) { return (inst){.op=LE , .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_sel(int x, int y, int z) { return (inst){.op=SEL, .x=x, .y=y, .z=z, .hi=-1}.bits; }

uint32_t rbb_call(int ix) { return (inst){.op=CALL, .x=ix, .hi=-1}.bits; }
#pragma GCC diagnostic pop


void eval(struct rbb const *rbb, float reg[64], struct rbb const *const call[64]) {
    float const T = 1.0f,
                F = 0.0f;
    for (int i = rbb->in; i < rbb->insts; i++) {
        inst const inst = {.bits=rbb->inst[i]};

        if (inst.imm <= inst.imm) {
            reg[i] = inst.imm;
        } else switch (inst.op) {
            default: __builtin_unreachable();

            case  ABS: reg[i] = fabsf(reg[inst.x]); break;
            case  NEG: reg[i] =     -(reg[inst.x]); break;
            case SQRT: reg[i] = sqrtf(reg[inst.x]); break;

            case ADD: reg[i] = reg[inst.x] + reg[inst.y]; break;
            case SUB: reg[i] = reg[inst.x] - reg[inst.y]; break;
            case MUL: reg[i] = reg[inst.x] * reg[inst.y]; break;
            case DIV: reg[i] = reg[inst.x] / reg[inst.y]; break;
            case FMA: reg[i] = fmaf(reg[inst.x], reg[inst.y], reg[inst.z]); break;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wfloat-equal"
            case EQ:  reg[i] = reg[inst.x] == reg[inst.y] ? T : F; break;
            case NE:  reg[i] = reg[inst.x] != reg[inst.y] ? T : F; break;
            case LT:  reg[i] = reg[inst.x] <  reg[inst.y] ? T : F; break;
            case LE:  reg[i] = reg[inst.x] <= reg[inst.y] ? T : F; break;
            case SEL: reg[i] = reg[inst.x] == T ? reg[inst.y] : reg[inst.z]; break;
        #pragma GCC diagnostic pop

            case CALL: {
                struct rbb const *sub = call[inst.x];
                int   const in  = sub->in;
                int   const out = sub->out;
                float subreg[64] = {0};
                for (int j = 0; j < in; j++) {
                    subreg[j] = reg[i - in + j];
                }
                eval(sub, subreg, call);
                for (int j = 0; j < out; j++) {
                    reg[i + j] = subreg[sub->insts - out + j];
                }
                i += out - 1;
            } break;
        }
    }
}
