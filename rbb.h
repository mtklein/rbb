#pragma once

#include <stddef.h>
#include <stdint.h>

typedef _Float16 v4h __attribute__((ext_vector_type(4)));
typedef _Float16 v8h __attribute__((ext_vector_type(8)));
typedef float    v8f __attribute__((ext_vector_type(8)));
typedef short    v8s __attribute__((ext_vector_type(8)));
typedef int      v8i __attribute__((ext_vector_type(8)));

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

struct rbb_meta {
    int inputs, outputs, registers;
    _Bool jit_f, jit_h, pad[2];
};

struct rbb*     rbb(struct rbb_inst const inst[], int insts);
struct rbb_meta rbb_meta(struct rbb const*);
void            rbb_eval_f(struct rbb const*, v8f reg[]);
void            rbb_eval_h(struct rbb const*, v8h reg[]);
void            rbb_free(struct rbb*);

struct cfg {
    void (*eval_f)(struct cfg const*, v8f reg[]);
    void (*eval_h)(struct cfg const*, v8h reg[]);
};
void cfg_eval_f(struct cfg const*, v8f reg[]);
void cfg_eval_h(struct cfg const*, v8h reg[]);

struct cfg_load {
    struct cfg  cfg;
    void const *src;
};
struct cfg_load load_565    (uint16_t const*);
struct cfg_load load_8888   (uint32_t const*);
struct cfg_load load_1010102(uint32_t const*);
struct cfg_load load_fp16   (v4h      const*);

struct cfg_store {
    struct cfg        cfg;
    void             *dst;
    struct cfg const *input;
};
struct cfg_store store_565    (uint16_t*, struct cfg const *rgb);
struct cfg_store store_8888   (uint32_t*, struct cfg const *rgba);
struct cfg_store store_1010102(uint32_t*, struct cfg const *rgba);
struct cfg_store store_fp16   (     v4h*, struct cfg const *rgba);

struct cfg_rbb {
    struct cfg        cfg;
    struct rbb const *rbb;
    struct cfg const *input;
};
struct cfg_rbb cfg_rbb(struct rbb const*, struct cfg const *input);
