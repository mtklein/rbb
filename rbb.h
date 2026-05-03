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
    int    inputs, outputs, registers, :32;
    size_t jit_size;
};

struct rbb*     rbb(struct rbb_inst const inst[], int insts);
struct rbb_meta rbb_meta(struct rbb const*);
void            rbb_eval(struct rbb const*, v8f reg[]);
_Bool           rbb_jit (struct rbb const*, void*);
void            rbb_free(struct rbb*);
