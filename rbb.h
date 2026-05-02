#pragma once

#include <stdint.h>

uint32_t rbb_imm(float);

uint32_t rbb_abs  (int x);
uint32_t rbb_neg  (int x);
uint32_t rbb_sqrt (int x);
uint32_t rbb_floor(int x);
uint32_t rbb_ceil (int x);
uint32_t rbb_trunc(int x);
uint32_t rbb_round(int x);

uint32_t rbb_add(int x, int y);
uint32_t rbb_sub(int x, int y);
uint32_t rbb_mul(int x, int y);
uint32_t rbb_div(int x, int y);
uint32_t rbb_min(int x, int y);
uint32_t rbb_max(int x, int y);
uint32_t rbb_fma(int x, int y, int z);

uint32_t rbb_eq(int x, int y);
uint32_t rbb_ne(int x, int y);
uint32_t rbb_lt(int x, int y);
uint32_t rbb_le(int x, int y);

uint32_t rbb_not(int x);
uint32_t rbb_and(int x, int y);
uint32_t rbb_or (int x, int y);
uint32_t rbb_xor(int x, int y);
uint32_t rbb_sel(int x, int y, int z);

uint32_t rbb_call(int ix);

struct rbb {
    int      in, out;
    int      insts;
    uint32_t inst[64];
};

int rbb_inline(struct rbb *dst, struct rbb const *src, int const args[]);

void eval(struct rbb const*, float reg[64], struct rbb const *const call[64]);

typedef float v8f __attribute__((ext_vector_type(8)));
void evalv(struct rbb const*, v8f reg[64], struct rbb const *const call[64]);
