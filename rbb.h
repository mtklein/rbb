#pragma once

#include <stddef.h>
#include <stdint.h>

typedef float    v8f __attribute__((ext_vector_type(8)));
typedef _Float16 v8h __attribute__((ext_vector_type(8)));
typedef int      v8i __attribute__((ext_vector_type(8)));
typedef short    v8s __attribute__((ext_vector_type(8)));

enum rbb_op {
    IMM, CALL,
    NEG, ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND, NOT,
    ADD, SUB, MUL, DIV, MIN, MAX, EQ, GT, GE, AND, OR, XOR,
    FMA, SEL,
};

struct rbb_inst {
    enum rbb_op       op :8;
    uint8_t           x,y,d;
    float             imm;
    struct rbb const *call;
};

struct cfg_meta {
    int inputs, outputs, registers;
    _Bool jit_f, jit_h, pad[2];
};

struct rbb*     rbb(struct rbb_inst const inst[], int insts);
struct cfg_meta rbb_meta(struct rbb const*);
void            rbb_eval_f(struct rbb const*, v8f reg[]);
void            rbb_eval_h(struct rbb const*, v8h reg[]);
void            rbb_free(struct rbb*);

struct cfg {
    void            (*free  )(struct cfg*);
    void            (*eval_f)(struct cfg const*, v8f reg[]);
    void            (*eval_h)(struct cfg const*, v8h reg[]);
    struct cfg_meta (*meta  )(struct cfg const*);
};
void cfg_free(struct cfg*);

struct rgba_fp16 {
    _Float16 r,g,b,a;
};

struct cfg* load_565    (        uint16_t const*);
struct cfg* load_8888   (        uint32_t const*);
struct cfg* load_1010102(        uint32_t const*);
struct cfg* load_fp16   (struct rgba_fp16 const*);

struct cfg* store_565    (        uint16_t*, struct cfg *rgb);
struct cfg* store_8888   (        uint32_t*, struct cfg *rgba);
struct cfg* store_1010102(        uint32_t*, struct cfg *rgba);
struct cfg* store_fp16   (struct rgba_fp16*, struct cfg *rgba);
