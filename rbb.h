#pragma once

#include <stdint.h>

typedef _Float16 v8h __attribute__((ext_vector_type(8)));
typedef short    v8s __attribute__((ext_vector_type(8)));

enum rbb_op : uint8_t {
    IMM,
    NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND, NOT,
    ADD, SUB, MUL, DIV, MIN, MAX, EQ, GT, GE, AND, OR, XOR,
    FMA, SEL,
};

struct rbb_inst {
    enum rbb_op op;
    uint8_t     d;
    union {
        struct { uint8_t x,y; };
        _Float16 imm;
    };
};

struct rbb* rbb     (struct rbb_inst const inst[], int insts);
int         rbb_regs(struct rbb const*);
void        rbb_eval(struct rbb const*, v8h reg[]);
void        rbb_free(struct rbb*);
