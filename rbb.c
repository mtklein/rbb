#include "rbb.h"
#include <math.h>

enum {
    ABS=1, NEG, SQRT,
    ADD, SUB, MUL, DIV, FMA,
    EQ, NE, LT, LE,
    AND, OR, NOT, SEL,
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

uint32_t rbb_eq(int x, int y) { return (inst){.op=EQ , .x=x, .y=y, .hi=-1}.bits; }
uint32_t rbb_ne(int x, int y) { return (inst){.op=NE , .x=x, .y=y, .hi=-1}.bits; }
uint32_t rbb_lt(int x, int y) { return (inst){.op=LT , .x=x, .y=y, .hi=-1}.bits; }
uint32_t rbb_le(int x, int y) { return (inst){.op=LE , .x=x, .y=y, .hi=-1}.bits; }

uint32_t rbb_not(int x              ) { return (inst){.op=NOT, .x=x,             .hi=-1}.bits; }
uint32_t rbb_and(int x, int y       ) { return (inst){.op=AND, .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_or (int x, int y       ) { return (inst){.op=OR , .x=x, .y=y,       .hi=-1}.bits; }
uint32_t rbb_sel(int x, int y, int z) { return (inst){.op=SEL, .x=x, .y=y, .z=z, .hi=-1}.bits; }

uint32_t rbb_call(int ix) { return (inst){.op=CALL, .x=ix, .hi=-1}.bits; }

int rbb_inline(struct rbb *dst, struct rbb const *src, int const args[]) {
    int const offset = dst->insts - src->in;
    for (int k = src->in; k < src->insts; k++) {
        inst si = {.bits = src->inst[k]};
        if (!(si.imm <= si.imm) && si.op != CALL) {
            si.x = si.x < (uint32_t)src->in ? (uint32_t)args[si.x] : si.x + (uint32_t)offset;
            si.y = si.y < (uint32_t)src->in ? (uint32_t)args[si.y] : si.y + (uint32_t)offset;
            si.z = si.z < (uint32_t)src->in ? (uint32_t)args[si.z] : si.z + (uint32_t)offset;
        }
        dst->inst[dst->insts++] = si.bits;
    }
    return dst->insts - src->out;
}
#pragma GCC diagnostic pop


typedef int v8i __attribute__((ext_vector_type(8)));

static uint32_t f2u(float    x) { return __builtin_bit_cast(uint32_t, x); }
static float    u2f(uint32_t x) { return __builtin_bit_cast(float, x); }
static v8i      vf2u(v8f x) { return __builtin_bit_cast(v8i, x); }
static v8f      vu2f(v8i x) { return __builtin_bit_cast(v8f, x); }

void evalv(struct rbb const *rbb, v8f reg[64], struct rbb const *const call[64]) {
    for (int i = rbb->in; i < rbb->insts; i++) {
        inst const inst = {.bits=rbb->inst[i]};

        if (inst.imm <= inst.imm) {
            reg[i] = inst.imm;
        } else switch (inst.op) {
            default: __builtin_unreachable();

            case  ABS: reg[i] = __builtin_elementwise_abs (reg[inst.x]); break;
            case  NEG: reg[i] =                          -(reg[inst.x]); break;
            case SQRT: reg[i] = __builtin_elementwise_sqrt(reg[inst.x]); break;

            case ADD: reg[i] = reg[inst.x] + reg[inst.y]; break;
            case SUB: reg[i] = reg[inst.x] - reg[inst.y]; break;
            case MUL: reg[i] = reg[inst.x] * reg[inst.y]; break;
            case DIV: reg[i] = reg[inst.x] / reg[inst.y]; break;
            case FMA: reg[i] = __builtin_elementwise_fma(reg[inst.x], reg[inst.y], reg[inst.z]);
                               break;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wfloat-equal"
            case EQ:  reg[i] = vu2f(reg[inst.x] == reg[inst.y]); break;
            case NE:  reg[i] = vu2f(reg[inst.x] != reg[inst.y]); break;
            case LT:  reg[i] = vu2f(reg[inst.x] <  reg[inst.y]); break;
            case LE:  reg[i] = vu2f(reg[inst.x] <= reg[inst.y]); break;
        #pragma GCC diagnostic pop

            case AND: reg[i] = vu2f( vf2u(reg[inst.x]) & vf2u(reg[inst.y])); break;
            case OR : reg[i] = vu2f( vf2u(reg[inst.x]) | vf2u(reg[inst.y])); break;
            case NOT: reg[i] = vu2f(~vf2u(reg[inst.x])); break;
            case SEL: {
                v8i const mask = vf2u(reg[inst.x]);
                reg[i] = vu2f( ( mask & vf2u(reg[inst.y]))
                             | (~mask & vf2u(reg[inst.z])));
            } break;

            case CALL: {
                struct rbb const *sub = call[inst.x];
                int const in  = sub->in;
                int const out = sub->out;
                v8f subreg[64] = {0};
                for (int j = 0; j < in; j++) {
                    subreg[j] = reg[i - in + j];
                }
                evalv(sub, subreg, call);
                for (int j = 0; j < out; j++) {
                    reg[i + j] = subreg[sub->insts - out + j];
                }
                i += out - 1;
            } break;
        }
    }
}

void eval(struct rbb const *rbb, float reg[64], struct rbb const *const call[64]) {
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
            case EQ:  reg[i] = u2f(reg[inst.x] == reg[inst.y] ? ~0u : 0u); break;
            case NE:  reg[i] = u2f(reg[inst.x] != reg[inst.y] ? ~0u : 0u); break;
            case LT:  reg[i] = u2f(reg[inst.x] <  reg[inst.y] ? ~0u : 0u); break;
            case LE:  reg[i] = u2f(reg[inst.x] <= reg[inst.y] ? ~0u : 0u); break;
        #pragma GCC diagnostic pop

            case AND: reg[i] = u2f( f2u(reg[inst.x]) & f2u(reg[inst.y])); break;
            case OR : reg[i] = u2f( f2u(reg[inst.x]) | f2u(reg[inst.y])); break;
            case NOT: reg[i] = u2f(~f2u(reg[inst.x])); break;
            case SEL: {
                uint32_t m = f2u(reg[inst.x]);
                reg[i] = u2f((m & f2u(reg[inst.y])) | (~m & f2u(reg[inst.z])));
            } break;

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
