#pragma once

#include <stddef.h>
#include <stdint.h>

// TODO: single-pump v8f JIT when it can't fit in double-pump registers?
// TODO: use natural register sizes v4f, v8h?
// TODO: LT,LE -> GT,GE?

typedef float    v8f __attribute__((ext_vector_type(8)));
typedef _Float16 v8h __attribute__((ext_vector_type(8)));
typedef int      v8i __attribute__((ext_vector_type(8)));
typedef short    v8s __attribute__((ext_vector_type(8)));

enum rbb_op {
    IMM,
    NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND,
    ADD, SUB, MUL, DIV, MIN, MAX, FMA,
    EQ, LT, LE,
    AND, OR, XOR, NOT, SEL,
    CALL,
};

struct rbb;

struct rbb_inst {
    enum rbb_op       op :8;
    uint8_t           x,y,d;
    float             imm;
    struct rbb const *call;
};

struct rbb_meta {
    int inputs, outputs, registers;
    _Bool jit_f, jit_h, pad[2];
};

struct rbb*     rbb(struct rbb_inst const inst[], int insts);
struct rbb_meta rbb_meta(struct rbb const*);
void            rbb_eval_f(struct rbb const*, v8f reg[]);
void            rbb_eval_h(struct rbb const*, v8h reg[]);
void            rbb_free(struct rbb*);
