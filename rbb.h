#pragma once

#include <stdint.h>

typedef float v8f __attribute__((ext_vector_type(8)));

enum rbb_op {
    IMM,
    NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND,
    ADD, SUB, MUL, DIV, MIN, MAX, FMA,
    EQ, NE, LT, LE,
    AND, OR, XOR, NOT, SEL,
};

struct rbb_inst {
    enum rbb_op op : 8;
    uint8_t d, :8, :8;
    union {
        struct { uint8_t x,y,z; };
        float imm;
    };
};

struct rbb* rbb(struct rbb_inst const inst[], int insts);
void   rbb_free(struct rbb*);
void   rbb_eval(struct rbb const*, v8f reg[]);
