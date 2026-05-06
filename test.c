#include "rbb.h"
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)
#define count(arr) (int)( sizeof arr / sizeof 0[arr] )

static const float    Tf = ((v8f)(v8i)-1).x;
static const _Float16 Th = ((v8h)(v8s)-1).x;

static _Bool exact_f(float x, float y) {
    union { float f; int bits; } X={x}, Y={y};
    return X.bits == Y.bits;
}
static _Bool exact_h(_Float16 x, _Float16 y) {
    union { _Float16 h; short bits; } X={x}, Y={y};
    return X.bits == Y.bits;
}

static void test_IMM(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=3.5}, 1);
    { v8f reg[] = {0}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 3.5) here; }
    { v8h reg[] = {0}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 3.5) here; }
    rbb_free(bb);
}

static void test_NEG(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    { v8f reg[] = {42}; rbb_eval_f(bb, reg); exact_f(reg[0].x, -42) here; }
    { v8h reg[] = {42}; rbb_eval_h(bb, reg); exact_h(reg[0].x, -42) here; }
    rbb_free(bb);
}

static void test_ABS(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ABS, .d=0, .x=0}, 1);
    { v8f reg[] = {-42}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 42) here; }
    { v8h reg[] = {-42}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 42) here; }
    rbb_free(bb);
}

static void test_SQRT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SQRT, .d=0, .x=0}, 1);
    { v8f reg[] = {49}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 7) here; }
    { v8h reg[] = {49}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 7) here; }
    rbb_free(bb);
}

static void test_FLOOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FLOOR, .d=0, .x=0}, 1);
    { v8f reg[] = {1.75}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 1) here; }
    { v8h reg[] = {1.75}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 1) here; }
    rbb_free(bb);
}

static void test_CEIL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=CEIL, .d=0, .x=0}, 1);
    { v8f reg[] = {1.25}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 2) here; }
    { v8h reg[] = {1.25}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 2) here; }
    rbb_free(bb);
}

static void test_TRUNC(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=TRUNC, .d=0, .x=0}, 1);
    { v8f reg[] = {-1.75}; rbb_eval_f(bb, reg); exact_f(reg[0].x, -1) here; }
    { v8h reg[] = {-1.75}; rbb_eval_h(bb, reg); exact_h(reg[0].x, -1) here; }
    rbb_free(bb);
}

static void test_ROUND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ROUND, .d=0, .x=0}, 1);
    { v8f reg[] = {1.5}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 2) here; }
    { v8h reg[] = {1.5}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 2) here; }
    rbb_free(bb);
}

static void test_ADD(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {42, 47}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 89) here; }
    { v8h reg[] = {42, 47}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 89) here; }
    rbb_free(bb);
}

static void test_SUB(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SUB, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {47, 42}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 5) here; }
    { v8h reg[] = {47, 42}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 5) here; }
    rbb_free(bb);
}

static void test_MUL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MUL, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {6, 7}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 42) here; }
    { v8h reg[] = {6, 7}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 42) here; }
    rbb_free(bb);
}

static void test_DIV(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=DIV, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {84, 2}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 42) here; }
    { v8h reg[] = {84, 2}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 42) here; }
    rbb_free(bb);
}

static void test_MIN(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MIN, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {3, 5}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 3) here; }
    { v8h reg[] = {3, 5}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 3) here; }
    rbb_free(bb);
}

static void test_MAX(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MAX, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {3, 5}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 5) here; }
    { v8h reg[] = {3, 5}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 5) here; }
    rbb_free(bb);
}

static void test_FMA(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FMA, .d=0, .x=1, .y=2}, 1);
    { v8f reg[] = {4, 2, 3}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 10) here; }
    { v8h reg[] = {4, 2, 3}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 10) here; }
    rbb_free(bb);
}

static void test_EQ(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=EQ, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {42, 42}; rbb_eval_f(bb, reg); exact_f(reg[0].x, Tf) here; }
    { v8h reg[] = {42, 42}; rbb_eval_h(bb, reg); exact_h(reg[0].x, Th) here; }
    rbb_free(bb);
}

static void test_GT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=GT, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {5, 3}; rbb_eval_f(bb, reg); exact_f(reg[0].x, Tf) here; }
    { v8h reg[] = {5, 3}; rbb_eval_h(bb, reg); exact_h(reg[0].x, Th) here; }
    rbb_free(bb);
}

static void test_GE(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=GE, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {5, 5}; rbb_eval_f(bb, reg); exact_f(reg[0].x, Tf) here; }
    { v8h reg[] = {5, 5}; rbb_eval_h(bb, reg); exact_h(reg[0].x, Th) here; }
    rbb_free(bb);
}

static void test_AND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=AND, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {Tf, 0}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 0) here; }
    { v8h reg[] = {Th, 0}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 0) here; }
    rbb_free(bb);
}

static void test_OR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=OR, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {Tf, 0}; rbb_eval_f(bb, reg); exact_f(reg[0].x, Tf) here; }
    { v8h reg[] = {Th, 0}; rbb_eval_h(bb, reg); exact_h(reg[0].x, Th) here; }
    rbb_free(bb);
}

static void test_XOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=XOR, .d=0, .x=0, .y=1}, 1);
    { v8f reg[] = {Tf, Tf}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 0) here; }
    { v8h reg[] = {Th, Th}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 0) here; }
    rbb_free(bb);
}

static void test_NOT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NOT, .d=0, .x=0}, 1);
    { v8f reg[] = {0}; rbb_eval_f(bb, reg); exact_f(reg[0].x, Tf) here; }
    { v8h reg[] = {0}; rbb_eval_h(bb, reg); exact_h(reg[0].x, Th) here; }
    rbb_free(bb);
}

static void test_SEL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SEL, .d=0, .x=1, .y=2}, 1);
    { v8f reg[] = {Tf, 1, 2}; rbb_eval_f(bb, reg); exact_f(reg[0].x, 1) here; }
    { v8h reg[] = {Th, 1, 2}; rbb_eval_h(bb, reg); exact_h(reg[0].x, 1) here; }
    rbb_free(bb);
}

// regs > 16 forces the f4 (v4f x 2) JIT path; result must be the same as f8.
static void test_f4_path(void) {
    struct rbb_inst const inst[] = {
        {.op=IMM, .d=16, .imm=3.5f},     // forces regs = 17 → f4
        {.op=NEG, .d=0,  .x=16},
    };
    struct rbb *bb = rbb(inst, count(inst));
    rbb_regs(bb) == 17 here;

    v8f reg[17] = {0};
    rbb_eval_f(bb, reg);
    for (int i = 0; i < 8; i++) { exact_f(reg[0][i], -3.5f) here; }
    rbb_free(bb);
}

static void test_regs_empty(void) {
    struct rbb *bb = rbb(NULL, 0);
    rbb_regs(bb) == 0 here;
    rbb_free(bb);
}

static void test_regs_IMM(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=42}, 1);
    rbb_regs(bb) == 1 here;
    rbb_free(bb);
}

static void test_regs_repeated_source(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    rbb_regs(bb) == 1 here;
    rbb_free(bb);
}

static void test_regs_in_place(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    rbb_regs(bb) == 1 here;
    rbb_free(bb);
}

static void test_regs_gap(void) {
    struct rbb_inst const inst[] = {
        {.op=IMM, .d=2, .imm=1},
        {.op=NEG, .d=0, .x=2},
    };
    struct rbb *bb = rbb(inst, 2);
    rbb_regs(bb) == 3 here;
    rbb_free(bb);
}

int main(void) {
    test_IMM();
    test_NEG();
    test_ABS();
    test_SQRT();
    test_FLOOR();
    test_CEIL();
    test_TRUNC();
    test_ROUND();
    test_ADD();
    test_SUB();
    test_MUL();
    test_DIV();
    test_MIN();
    test_MAX();
    test_FMA();
    test_EQ();
    test_GT();
    test_GE();
    test_AND();
    test_OR();
    test_XOR();
    test_NOT();
    test_SEL();
    test_f4_path();

    test_regs_empty();
    test_regs_IMM();
    test_regs_repeated_source();
    test_regs_in_place();
    test_regs_gap();
    return 0;
}
