#pragma once

#include <stddef.h>

typedef float v8f __attribute__((ext_vector_type(8)));

enum rbb_op {
    IMM,
    NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND,
    ADD, SUB, MUL, DIV, MIN, MAX, FMA,
    EQ, NE, LT, LE,
    AND, OR, XOR, NOT, SEL,
    CALL,
};

struct rbb;

struct rbb_inst {
    enum rbb_op       op;
    short             d,x,y,z;
    float             imm;
    struct rbb const *call;
};

struct rbb_meta {
    int inputs, outputs, registers;
    _Bool jit, pad[3];
};

struct rbb*     rbb(struct rbb_inst const inst[], int insts);
struct rbb_meta rbb_meta(struct rbb const*);
void            rbb_eval(struct rbb const*, v8f reg[]);
void            rbb_free(struct rbb*);
