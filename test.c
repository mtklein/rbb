#include "rbb.h"
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static void test_imm(void) {
    struct rbb r = {0,4, {
        [0] = rbb_imm(    3.5f),
        [1] = rbb_imm(   -0.25f),
        [2] = rbb_imm(    0.0f),
        [3] = rbb_imm(-1234.5f),
    }};
    float reg[4];
    eval(&r, reg);
    equiv(reg[0],     3.5f) here;
    equiv(reg[1],    -0.25f) here;
    equiv(reg[2],     0.0f) here;
    equiv(reg[3], -1234.5f) here;
}

static void test_abs(void) {
    struct rbb r = {1,4, {
        [1] = rbb_imm(-2.5f),
        [2] = rbb_abs(0),
        [3] = rbb_abs(1),
    }};
    float reg[4] = {-7.0f};
    eval(&r, reg);
    equiv(reg[2], 7.0f) here;
    equiv(reg[3], 2.5f) here;
}

static void test_neg(void) {
    struct rbb r = {1,4, {
        [1] = rbb_imm(2.0f),
        [2] = rbb_neg(0),
        [3] = rbb_neg(1),
    }};
    float reg[4] = {3.0f};
    eval(&r, reg);
    equiv(reg[2], -3.0f) here;
    equiv(reg[3], -2.0f) here;
}

static void test_sqrt(void) {
    struct rbb r = {1,4, {
        [1] = rbb_imm(2.25f),
        [2] = rbb_sqrt(0),
        [3] = rbb_sqrt(1),
    }};
    float reg[4] = {16.0f};
    eval(&r, reg);
    equiv(reg[2], 4.0f) here;
    equiv(reg[3], 1.5f) here;
}

static void test_add(void) {
    struct rbb r = {2,3, {[2] = rbb_add(0,1)}};
    float reg[3] = {3.0f, -8.0f};
    eval(&r, reg);
    equiv(reg[2], -5.0f) here;
}

static void test_sub(void) {
    struct rbb r = {2,4, {
        [2] = rbb_sub(0,1),
        [3] = rbb_sub(1,0),
    }};
    float reg[4] = {10.0f, 3.0f};
    eval(&r, reg);
    equiv(reg[2],  7.0f) here;
    equiv(reg[3], -7.0f) here;
}

static void test_mul(void) {
    struct rbb r = {2,3, {[2] = rbb_mul(0,1)}};
    float reg[3] = {6.0f, -7.0f};
    eval(&r, reg);
    equiv(reg[2], -42.0f) here;
}

static void test_div(void) {
    struct rbb r = {2,4, {
        [2] = rbb_div(0,1),
        [3] = rbb_div(1,0),
    }};
    float reg[4] = {12.0f, 4.0f};
    eval(&r, reg);
    equiv(reg[2], 3.0f) here;
    equiv(reg[3], 1.0f/3.0f) here;
}

static void test_fma(void) {
    struct rbb r = {3,4, {[3] = rbb_fma(0,1,2)}};
    float reg[4] = {2.0f, 3.0f, 5.0f};
    eval(&r, reg);
    equiv(reg[3], 11.0f) here;
}

static void test_eq(void) {
    struct rbb r = {3,5, {
        [3] = rbb_eq(0,1),
        [4] = rbb_eq(0,2),
    }};
    float reg[5] = {3.0f, 3.0f, 4.0f};
    eval(&r, reg);
    equiv(reg[3], 1.0f) here;
    equiv(reg[4], 0.0f) here;
}

static void test_ne(void) {
    struct rbb r = {3,5, {
        [3] = rbb_ne(0,1),
        [4] = rbb_ne(0,2),
    }};
    float reg[5] = {3.0f, 3.0f, 4.0f};
    eval(&r, reg);
    equiv(reg[3], 0.0f) here;
    equiv(reg[4], 1.0f) here;
}

static void test_lt(void) {
    struct rbb r = {3,6, {
        [3] = rbb_lt(0,1),
        [4] = rbb_lt(1,0),
        [5] = rbb_lt(0,2),
    }};
    float reg[6] = {1.0f, 2.0f, 1.0f};
    eval(&r, reg);
    equiv(reg[3], 1.0f) here;
    equiv(reg[4], 0.0f) here;
    equiv(reg[5], 0.0f) here;
}

static void test_le(void) {
    struct rbb r = {3,6, {
        [3] = rbb_le(0,1),
        [4] = rbb_le(1,0),
        [5] = rbb_le(0,2),
    }};
    float reg[6] = {1.0f, 2.0f, 1.0f};
    eval(&r, reg);
    equiv(reg[3], 1.0f) here;
    equiv(reg[4], 0.0f) here;
    equiv(reg[5], 1.0f) here;
}

static void test_sel(void) {
    struct rbb r = {3,6, {
        [3] = rbb_imm(1.0f),
        [4] = rbb_sel(3,0,1),
        [5] = rbb_sel(2,0,1),
    }};
    float reg[6] = {100.0f, 200.0f, 0.0f};
    eval(&r, reg);
    equiv(reg[4], 100.0f) here;
    equiv(reg[5], 200.0f) here;
}

static void test_chain(void) {
    struct rbb foo = {
        2,7, {
            [2] = rbb_add(0,1),
            [3] = rbb_sub(0,1),
            [4] = rbb_mul(2,3),
            [5] = rbb_imm(-89),
            [6] = rbb_div(4,5),
        },
    };
    float reg[7] = {42,47};
    eval(&foo, reg);

    equiv(reg[0],   42) here;
    equiv(reg[1],   47) here;
    equiv(reg[2],   89) here;
    equiv(reg[3],   -5) here;
    equiv(reg[4], -445) here;
    equiv(reg[5],  -89) here;
    equiv(reg[6],    5) here;
}

static void test_max_capacity(void) {
    struct rbb r = {.in=1, .insts=64};
    float reg[64] = {0.0f};

    r.inst[1] = rbb_imm(1.0f);
    for (int i = 2; i < 64; i++) {
        r.inst[i] = rbb_add(i-1, 1);
    }
    eval(&r, reg);

    equiv(reg[0], 0.0f) here;
    for (int i = 1; i < 64; i++) {
        equiv(reg[i], (float)i) here;
    }
}

int main(void) {
    test_imm();
    test_abs();
    test_neg();
    test_sqrt();
    test_add();
    test_sub();
    test_mul();
    test_div();
    test_fma();
    test_eq();
    test_ne();
    test_lt();
    test_le();
    test_sel();
    test_chain();
    test_max_capacity();
    return 0;
}
