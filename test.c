#include "rbb.h"
#include <stdint.h>
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static _Bool bits_eq(float x, uint32_t b) {
    return __builtin_bit_cast(uint32_t, x) == b;
}

static void test_imm(void) {
    struct rbb r = {.in=0, .out=4, .insts=4, .inst={
        [0] = rbb_imm(    3.5f),
        [1] = rbb_imm(   -0.25f),
        [2] = rbb_imm(    0.0f),
        [3] = rbb_imm(-1234.5f),
    }};
    float reg[64];
    eval(&r, reg, NULL);
    equiv(reg[0],     3.5f) here;
    equiv(reg[1],    -0.25f) here;
    equiv(reg[2],     0.0f) here;
    equiv(reg[3], -1234.5f) here;
}

static void test_abs(void) {
    struct rbb r = {.in=1, .out=2, .insts=4, .inst={
        [1] = rbb_imm(-2.5f),
        [2] = rbb_abs(0),
        [3] = rbb_abs(1),
    }};
    float reg[64] = {-7.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], 7.0f) here;
    equiv(reg[3], 2.5f) here;
}

static void test_neg(void) {
    struct rbb r = {.in=1, .out=2, .insts=4, .inst={
        [1] = rbb_imm(2.0f),
        [2] = rbb_neg(0),
        [3] = rbb_neg(1),
    }};
    float reg[64] = {3.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], -3.0f) here;
    equiv(reg[3], -2.0f) here;
}

static void test_sqrt(void) {
    struct rbb r = {.in=1, .out=2, .insts=4, .inst={
        [1] = rbb_imm(2.25f),
        [2] = rbb_sqrt(0),
        [3] = rbb_sqrt(1),
    }};
    float reg[64] = {16.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], 4.0f) here;
    equiv(reg[3], 1.5f) here;
}

static void test_add(void) {
    struct rbb r = {.in=2, .out=1, .insts=3, .inst={
        [2] = rbb_add(0,1),
    }};
    float reg[64] = {3.0f, -8.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], -5.0f) here;
}

static void test_sub(void) {
    struct rbb r = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_sub(0,1),
        [3] = rbb_sub(1,0),
    }};
    float reg[64] = {10.0f, 3.0f};
    eval(&r, reg, NULL);
    equiv(reg[2],  7.0f) here;
    equiv(reg[3], -7.0f) here;
}

static void test_mul(void) {
    struct rbb r = {.in=2, .out=1, .insts=3, .inst={
        [2] = rbb_mul(0,1),
    }};
    float reg[64] = {6.0f, -7.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], -42.0f) here;
}

static void test_div(void) {
    struct rbb r = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_div(0,1),
        [3] = rbb_div(1,0),
    }};
    float reg[64] = {12.0f, 4.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], 3.0f) here;
    equiv(reg[3], 1.0f/3.0f) here;
}

static void test_min(void) {
    struct rbb r = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_min(0,1),
        [3] = rbb_min(1,0),
    }};
    float reg[64] = {3.0f, 7.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], 3.0f) here;
    equiv(reg[3], 3.0f) here;
}

static void test_max(void) {
    struct rbb r = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_max(0,1),
        [3] = rbb_max(1,0),
    }};
    float reg[64] = {3.0f, 7.0f};
    eval(&r, reg, NULL);
    equiv(reg[2], 7.0f) here;
    equiv(reg[3], 7.0f) here;
}

static void test_round_family(void) {
    struct rbb r = {.in=1, .out=4, .insts=5, .inst={
        [1] = rbb_floor(0),
        [2] = rbb_ceil (0),
        [3] = rbb_trunc(0),
        [4] = rbb_round(0),
    }};
    {
        float reg[64] = {2.6f};
        eval(&r, reg, NULL);
        equiv(reg[1], 2.0f) here;
        equiv(reg[2], 3.0f) here;
        equiv(reg[3], 2.0f) here;
        equiv(reg[4], 3.0f) here;
    }
    {
        float reg[64] = {-2.6f};
        eval(&r, reg, NULL);
        equiv(reg[1], -3.0f) here;
        equiv(reg[2], -2.0f) here;
        equiv(reg[3], -2.0f) here;
        equiv(reg[4], -3.0f) here;
    }
    {
        // Halfway cases: round() ties away from zero.
        float reg[64] = {1.5f};
        eval(&r, reg, NULL);
        equiv(reg[4], 2.0f) here;
    }
    {
        float reg[64] = {-1.5f};
        eval(&r, reg, NULL);
        equiv(reg[4], -2.0f) here;
    }
}

static void test_fma(void) {
    struct rbb r = {.in=3, .out=1, .insts=4, .inst={
        [3] = rbb_fma(0,1,2),
    }};
    float reg[64] = {2.0f, 3.0f, 5.0f};
    eval(&r, reg, NULL);
    equiv(reg[3], 11.0f) here;
}

static void test_eq(void) {
    struct rbb r = {.in=3, .out=2, .insts=5, .inst={
        [3] = rbb_eq(0,1),
        [4] = rbb_eq(0,2),
    }};
    float reg[64] = {3.0f, 3.0f, 4.0f};
    eval(&r, reg, NULL);
    bits_eq(reg[3], ~0u) here;
    bits_eq(reg[4],  0u) here;
}

static void test_ne(void) {
    struct rbb r = {.in=3, .out=2, .insts=5, .inst={
        [3] = rbb_ne(0,1),
        [4] = rbb_ne(0,2),
    }};
    float reg[64] = {3.0f, 3.0f, 4.0f};
    eval(&r, reg, NULL);
    bits_eq(reg[3],  0u) here;
    bits_eq(reg[4], ~0u) here;
}

static void test_lt(void) {
    struct rbb r = {.in=3, .out=3, .insts=6, .inst={
        [3] = rbb_lt(0,1),
        [4] = rbb_lt(1,0),
        [5] = rbb_lt(0,2),
    }};
    float reg[64] = {1.0f, 2.0f, 1.0f};
    eval(&r, reg, NULL);
    bits_eq(reg[3], ~0u) here;
    bits_eq(reg[4],  0u) here;
    bits_eq(reg[5],  0u) here;
}

static void test_le(void) {
    struct rbb r = {.in=3, .out=3, .insts=6, .inst={
        [3] = rbb_le(0,1),
        [4] = rbb_le(1,0),
        [5] = rbb_le(0,2),
    }};
    float reg[64] = {1.0f, 2.0f, 1.0f};
    eval(&r, reg, NULL);
    bits_eq(reg[3], ~0u) here;
    bits_eq(reg[4],  0u) here;
    bits_eq(reg[5], ~0u) here;
}

static void test_sel(void) {
    struct rbb r = {.in=3, .out=2, .insts=5, .inst={
        [3] = rbb_eq(0,0),       // ~0 mask
        [4] = rbb_sel(3, 0, 1),  // mask=~0 -> picks reg[0]
    }};
    float reg[64] = {100.0f, 200.0f, 0.0f};
    eval(&r, reg, NULL);
    equiv(reg[4], 100.0f) here;
}

static void test_bitwise(void) {
    struct rbb r = {.in=2, .out=4, .insts=6, .inst={
        [2] = rbb_eq (0,0),    // mask = ~0
        [3] = rbb_and(2,1),    // ~0 & v = v
        [4] = rbb_or (0,1),    //  0 | v = v
        [5] = rbb_not(0),      // ~0 = ~0
    }};
    float reg[64] = {0.0f, 0.5f};
    eval(&r, reg, NULL);
    bits_eq(reg[2], ~0u)  here;
    equiv  (reg[3], 0.5f) here;
    equiv  (reg[4], 0.5f) here;
    bits_eq(reg[5], ~0u)  here;
}

static void test_sel_blend(void) {
    // Use SEL as a real per-bit blend: pick lanes from y where mask is ~0, z where 0.
    // With scalar floats there are no lanes, but we still verify the pure-mask paths.
    struct rbb r = {.in=2, .out=2, .insts=5, .inst={
        [2] = rbb_eq (0,0),       // ~0 mask  -> picks y
        [3] = rbb_sel(2, 0, 1),
        [4] = rbb_sel(1, 0, 1),   //  0 mask (reg[1] bits = 0) -> picks z
    }};
    float reg[64] = {7.0f, 0.0f};  // reg[1] = 0.0f has all-zero bits
    eval(&r, reg, NULL);
    equiv(reg[3], 7.0f) here;
    equiv(reg[4], 0.0f) here;
}

static void test_chain(void) {
    struct rbb foo = {.in=2, .out=1, .insts=7, .inst={
        [2] = rbb_add(0,1),
        [3] = rbb_sub(0,1),
        [4] = rbb_mul(2,3),
        [5] = rbb_imm(-89),
        [6] = rbb_div(4,5),
    }};
    float reg[64] = {42,47};
    eval(&foo, reg, NULL);

    equiv(reg[0],   42) here;
    equiv(reg[1],   47) here;
    equiv(reg[2],   89) here;
    equiv(reg[3],   -5) here;
    equiv(reg[4], -445) here;
    equiv(reg[5],  -89) here;
    equiv(reg[6],    5) here;
}

static void test_max_capacity(void) {
    struct rbb r = {.in=1, .out=1, .insts=64};
    float reg[64] = {0.0f};

    r.inst[1] = rbb_imm(1.0f);
    for (int i = 2; i < 64; i++) {
        r.inst[i] = rbb_add(i-1, 1);
    }
    eval(&r, reg, NULL);

    equiv(reg[0], 0.0f) here;
    for (int i = 1; i < 64; i++) {
        equiv(reg[i], (float)i) here;
    }
}

static void test_call(void) {
    struct rbb dbl = {.in=1, .out=1, .insts=2, .inst={
        [1] = rbb_add(0,0),
    }};
    struct rbb caller = {.in=1, .out=1, .insts=3, .inst={
        [1] = rbb_call(0),
        [2] = rbb_call(0),
    }};
    struct rbb const *const calls[64] = { &dbl };

    float reg[64] = {3.0f};
    eval(&caller, reg, calls);

    equiv(reg[1],  6.0f) here;
    equiv(reg[2], 12.0f) here;
}

static void test_call_multi_io(void) {
    struct rbb sumprod = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_add(0,1),
        [3] = rbb_mul(0,1),
    }};
    struct rbb caller = {.in=2, .out=1, .insts=5, .inst={
        [2] = rbb_call(0),
        [4] = rbb_add(2,3),
    }};
    struct rbb const *const calls[64] = { &sumprod };

    float reg[64] = {2.0f, 3.0f};
    eval(&caller, reg, calls);

    equiv(reg[2],  5.0f) here;
    equiv(reg[3],  6.0f) here;
    equiv(reg[4], 11.0f) here;
}

static void test_inline(void) {
    struct rbb dbl = {.in=1, .out=1, .insts=2, .inst={
        [1] = rbb_add(0,0),
    }};
    struct rbb caller = {.in=1, .insts=1};
    float      reg[64] = {3.0f};
    int        r1 = rbb_inline(&caller, &dbl, (int[]){0});
    int        r2 = rbb_inline(&caller, &dbl, (int[]){r1});
    caller.out = 1;
    eval(&caller, reg, NULL);

    equiv(reg[r1],  6.0f) here;
    equiv(reg[r2], 12.0f) here;
}

static void test_inline_multi_io(void) {
    struct rbb sumprod = {.in=2, .out=2, .insts=4, .inst={
        [2] = rbb_add(0,1),
        [3] = rbb_mul(0,1),
    }};
    struct rbb caller = {.in=2, .insts=2};
    float      reg[64] = {2.0f, 3.0f};
    int        sum  = rbb_inline(&caller, &sumprod, (int[]){0, 1});
    int        prod = sum + 1;
    caller.inst[caller.insts++] = rbb_add(sum, prod);
    caller.out = 1;
    eval(&caller, reg, NULL);

    equiv(reg[sum],   5.0f) here;
    equiv(reg[prod],  6.0f) here;
    equiv(reg[caller.insts - 1], 11.0f) here;
}

static void test_inline_remap(void) {
    struct rbb shape = {.in=2, .out=1, .insts=4, .inst={
        [2] = rbb_mul(0,1),
        [3] = rbb_add(2,0),
    }};
    struct rbb caller = {.in=3, .insts=3};
    float      reg[64] = {99.0f, 4.0f, 5.0f};
    int        r = rbb_inline(&caller, &shape, (int[]){1, 2});
    caller.out = 1;
    eval(&caller, reg, NULL);

    equiv(reg[r], 24.0f) here;
}

static void test_evalv_chain(void) {
    struct rbb foo = {.in=2, .out=1, .insts=7, .inst={
        [2] = rbb_add(0,1),
        [3] = rbb_sub(0,1),
        [4] = rbb_mul(2,3),
        [5] = rbb_imm(-89),
        [6] = rbb_div(4,5),
    }};
    v8f reg[64] = {
        [0] = {42, 1, 2, 3, 4, 5, 6, 7},
        [1] = {47, 1, 2, 3, 4, 5, 6, 7},
    };
    evalv(&foo, reg, NULL);

    for (int j = 0; j < 8; j++) {
        float x = reg[0][j], y = reg[1][j];
        equiv(reg[2][j], x + y) here;
        equiv(reg[3][j], x - y) here;
        equiv(reg[4][j], (x + y) * (x - y)) here;
        equiv(reg[5][j], -89.0f) here;
        equiv(reg[6][j], (x + y) * (x - y) / -89.0f) here;
    }
}

static void test_evalv_unary(void) {
    struct rbb r = {.in=1, .out=3, .insts=4, .inst={
        [1] = rbb_abs (0),
        [2] = rbb_neg (0),
        [3] = rbb_sqrt(0),
    }};
    v8f reg[64] = {
        [0] = {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f, 0.25f, 100.0f},
    };
    evalv(&r, reg, NULL);
    for (int j = 0; j < 8; j++) {
        equiv(reg[1][j], reg[0][j]) here;
        equiv(reg[2][j], -reg[0][j]) here;
        equiv(reg[3][j]*reg[3][j], reg[0][j]) here;
    }
}

static void test_evalv_cmp_and_sel(void) {
    struct rbb r = {.in=2, .out=5, .insts=7, .inst={
        [2] = rbb_lt (0,1),
        [3] = rbb_le (0,1),
        [4] = rbb_eq (0,1),
        [5] = rbb_ne (0,1),
        [6] = rbb_sel(2,0,1),
    }};
    v8f reg[64] = {
        [0] = {1, 2, 3, 4, 5, 6, 7, 8},
        [1] = {2, 2, 2, 2, 5, 5, 5, 5},
    };
    evalv(&r, reg, NULL);
    for (int j = 0; j < 8; j++) {
        float x = reg[0][j], y = reg[1][j];
        bits_eq(reg[2][j],  x < y                ? ~0u : 0u) here;
        bits_eq(reg[3][j], (x < y || equiv(x,y)) ? ~0u : 0u) here;
        bits_eq(reg[4][j], equiv(x, y)           ? ~0u : 0u) here;
        bits_eq(reg[5][j], equiv(x, y)           ? 0u : ~0u) here;
        equiv  (reg[6][j],  x < y                ? x   : y ) here;
    }
}

static void test_evalv_bitwise(void) {
    struct rbb r = {.in=2, .out=4, .insts=6, .inst={
        [2] = rbb_eq (0,0),
        [3] = rbb_and(2,1),
        [4] = rbb_or (0,1),
        [5] = rbb_not(0),
    }};
    v8f reg[64] = {
        [0] = {0,0,0,0,0,0,0,0},
        [1] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
    };
    evalv(&r, reg, NULL);
    for (int j = 0; j < 8; j++) {
        bits_eq(reg[2][j], ~0u)        here;
        equiv  (reg[3][j], reg[1][j])  here;
        equiv  (reg[4][j], reg[1][j])  here;
        bits_eq(reg[5][j], ~0u)        here;
    }
}

static void test_evalv_round_minmax(void) {
    struct rbb r = {.in=2, .out=6, .insts=8, .inst={
        [2] = rbb_min  (0,1),
        [3] = rbb_max  (0,1),
        [4] = rbb_floor(0),
        [5] = rbb_ceil (0),
        [6] = rbb_trunc(0),
        [7] = rbb_round(0),
    }};
    v8f reg[64] = {
        [0] = {-2.6f, -1.5f, -0.5f, 0.0f, 0.5f, 1.5f, 2.6f, 3.0f},
        [1] = { 1.0f,  0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    };
    v8f expect_min   = {-2.6f, -1.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    v8f expect_max   = { 1.0f,  0.0f,  0.0f, 0.0f, 0.5f, 1.5f, 2.6f, 3.0f};
    v8f expect_floor = {-3.0f, -2.0f, -1.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f};
    v8f expect_ceil  = {-2.0f, -1.0f,  0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 3.0f};
    v8f expect_trunc = {-2.0f, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f};
    v8f expect_round = {-3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 3.0f};

    evalv(&r, reg, NULL);
    for (int j = 0; j < 8; j++) {
        equiv(reg[2][j], expect_min  [j]) here;
        equiv(reg[3][j], expect_max  [j]) here;
        equiv(reg[4][j], expect_floor[j]) here;
        equiv(reg[5][j], expect_ceil [j]) here;
        equiv(reg[6][j], expect_trunc[j]) here;
        equiv(reg[7][j], expect_round[j]) here;
    }
}

static void test_evalv_fma(void) {
    struct rbb r = {.in=3, .out=1, .insts=4, .inst={
        [3] = rbb_fma(0,1,2),
    }};
    v8f reg[64] = {
        [0] = {1, 2, 3, 4, 5, 6, 7, 8},
        [1] = {2, 2, 2, 2, 3, 3, 3, 3},
        [2] = {0, 1, 2, 3, 4, 5, 6, 7},
    };
    evalv(&r, reg, NULL);
    for (int j = 0; j < 8; j++) {
        equiv(reg[3][j], reg[0][j]*reg[1][j] + reg[2][j]) here;
    }
}

static void test_evalv_call(void) {
    struct rbb dbl = {.in=1, .out=1, .insts=2, .inst={
        [1] = rbb_add(0,0),
    }};
    struct rbb caller = {.in=1, .out=1, .insts=3, .inst={
        [1] = rbb_call(0),
        [2] = rbb_call(0),
    }};
    struct rbb const *const calls[64] = { &dbl };

    v8f reg[64] = {[0] = {1, 2, 3, 4, 5, 6, 7, 8}};
    evalv(&caller, reg, calls);

    for (int j = 0; j < 8; j++) {
        equiv(reg[1][j], reg[0][j] *  2.0f) here;
        equiv(reg[2][j], reg[0][j] *  4.0f) here;
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
    test_min();
    test_max();
    test_round_family();
    test_fma();
    test_eq();
    test_ne();
    test_lt();
    test_le();
    test_sel();
    test_bitwise();
    test_sel_blend();
    test_chain();
    test_max_capacity();
    test_call();
    test_call_multi_io();
    test_inline();
    test_inline_multi_io();
    test_inline_remap();
    test_evalv_chain();
    test_evalv_unary();
    test_evalv_cmp_and_sel();
    test_evalv_bitwise();
    test_evalv_round_minmax();
    test_evalv_fma();
    test_evalv_call();
    return 0;
}
