#pragma once

#include <stdint.h>

typedef _Float16 v8h __attribute__((ext_vector_type(8)));
typedef float    v8f __attribute__((ext_vector_type(8)));
typedef short    v8s __attribute__((ext_vector_type(8)));
typedef int      v8i __attribute__((ext_vector_type(8)));

enum rbb_op {
    IMM, LOAD,
    STORE, NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND, NOT,
    ADD, SUB, MUL, DIV, MIN, MAX, EQ, GT, GE, AND, OR, XOR,
    FMA, SEL,
};

struct rbb_inst {
    enum rbb_op op :8;
    uint8_t     d,x,y;
    float       imm;
};

struct rbb* rbb(struct rbb_inst const inst[], int insts);
int    rbb_regs  (struct rbb const*);
int    rbb_ptrs  (struct rbb const*);
void   rbb_eval_f(struct rbb const*, v8f reg[], void *ptr[]);
void   rbb_eval_h(struct rbb const*, v8h reg[], void *ptr[]);
void   rbb_free  (struct rbb*);
