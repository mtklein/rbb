#pragma once

#include <stdint.h>

uint32_t rbb_imm(float);

uint32_t rbb_abs (int x);
uint32_t rbb_neg (int x);
uint32_t rbb_sqrt(int x);

uint32_t rbb_add(int x, int y);
uint32_t rbb_sub(int x, int y);
uint32_t rbb_mul(int x, int y);
uint32_t rbb_div(int x, int y);
uint32_t rbb_fma(int x, int y, int z);

uint32_t rbb_eq (int x, int y);
uint32_t rbb_ne (int x, int y);
uint32_t rbb_lt (int x, int y);
uint32_t rbb_le (int x, int y);
uint32_t rbb_sel(int x, int y, int z);

struct rbb {
    int      in, out;
    int      insts;
    uint32_t inst[64];
};

void eval(struct rbb const*, float reg[64]);
