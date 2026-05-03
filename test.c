#include "rbb.h"
#include <stdint.h>
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

typedef int v8i __attribute__((ext_vector_type(8)));
static const float T = ((v8f)(v8i)-1).x;

static _Bool exact(float x, float y) {
    union {
        float    f;
        uint32_t bits;
    } X={x}, Y={y};
    return X.bits == Y.bits;
}

static void test_IMM(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=3.14f}, 1);
    v8f reg[] = {0};
    rbb_eval(bb, reg);
    exact(reg[0].x, 3.14f) here;
    rbb_free(bb);
}

static void test_NEG(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    v8f reg[] = {42};
    rbb_eval(bb, reg);
    exact(reg[0].x, -42) here;
    rbb_free(bb);
}

static void test_ABS(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ABS, .d=0, .x=0}, 1);
    v8f reg[] = {-42};
    rbb_eval(bb, reg);
    exact(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_SQRT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SQRT, .d=0, .x=0}, 1);
    v8f reg[] = {49};
    rbb_eval(bb, reg);
    exact(reg[0].x, 7) here;
    rbb_free(bb);
}

static void test_FLOOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FLOOR, .d=0, .x=0}, 1);
    v8f reg[] = {1.7f};
    rbb_eval(bb, reg);
    exact(reg[0].x, 1) here;
    rbb_free(bb);
}

static void test_CEIL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=CEIL, .d=0, .x=0}, 1);
    v8f reg[] = {1.3f};
    rbb_eval(bb, reg);
    exact(reg[0].x, 2) here;
    rbb_free(bb);
}

static void test_TRUNC(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=TRUNC, .d=0, .x=0}, 1);
    v8f reg[] = {-1.7f};
    rbb_eval(bb, reg);
    exact(reg[0].x, -1) here;
    rbb_free(bb);
}

static void test_ROUND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ROUND, .d=0, .x=0}, 1);
    v8f reg[] = {1.5f};
    rbb_eval(bb, reg);
    exact(reg[0].x, 2) here;
    rbb_free(bb);
}

static void test_ADD(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);

    v8f reg[] = {42, 47};
    rbb_eval(bb, reg);

    exact(reg[0].x, 89) here;
    exact(reg[1].y, 47) here;

    rbb_free(bb);
}

static void test_SUB(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SUB, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {47, 42};
    rbb_eval(bb, reg);
    exact(reg[0].x, 5) here;
    rbb_free(bb);
}

static void test_MUL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MUL, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {6, 7};
    rbb_eval(bb, reg);
    exact(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_DIV(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=DIV, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {84, 2};
    rbb_eval(bb, reg);
    exact(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_MIN(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MIN, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval(bb, reg);
    exact(reg[0].x, 3) here;
    rbb_free(bb);
}

static void test_MAX(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MAX, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval(bb, reg);
    exact(reg[0].x, 5) here;
    rbb_free(bb);
}

static void test_FMA(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FMA, .d=0, .x=0, .y=1, .z=2}, 1);
    v8f reg[] = {2, 3, 4};
    rbb_eval(bb, reg);
    exact(reg[0].x, 10) here;
    rbb_free(bb);
}

static void test_EQ(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=EQ, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {42, 42};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_NE(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NE, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {42, 47};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_LT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LT, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_LE(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LE, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {5, 5};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_AND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=AND, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {T, 0};
    rbb_eval(bb, reg);
    exact(reg[0].x, 0) here;
    rbb_free(bb);
}

static void test_OR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=OR, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {T, 0};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_XOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=XOR, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {T, T};
    rbb_eval(bb, reg);
    exact(reg[0].x, 0) here;
    rbb_free(bb);
}

static void test_NOT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NOT, .d=0, .x=0}, 1);
    v8f reg[] = {0};
    rbb_eval(bb, reg);
    exact(reg[0].x, T) here;
    rbb_free(bb);
}

static void test_SEL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SEL, .d=0, .x=0, .y=1, .z=2}, 1);
    v8f reg[] = {T, 1, 2};
    rbb_eval(bb, reg);
    exact(reg[0].x, 1) here;
    rbb_free(bb);
}

static void test_CALL(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8f reg[] = {21};
    rbb_eval(caller, reg);
    exact(reg[0].x, 42) here;

    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_two_args(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8f reg[] = {10, 32};
    rbb_eval(caller, reg);
    exact(reg[0].x, 42) here;

    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_then_op(void) {
    struct rbb *doubler = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller  = rbb((struct rbb_inst[]){
        {.op=CALL, .d=0, .call=doubler},
        {.op=NEG,  .d=0, .x=0},
    }, 2);

    v8f reg[] = {21};
    rbb_eval(caller, reg);
    exact(reg[0].x, -42) here;

    rbb_free(caller);
    rbb_free(doubler);
}

static void check_meta(struct rbb const *bb, int inputs, int outputs, int registers) {
    struct rbb_meta const m = rbb_meta(bb);
    m.inputs    == inputs    here;
    m.outputs   == outputs   here;
    m.registers == registers here;
}

static void test_meta_empty(void) {
    struct rbb *bb = rbb(NULL, 0);
    check_meta(bb, 0,0,0);
    rbb_free(bb);
}

static void test_meta_no_inputs(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=42}, 1);
    check_meta(bb, 0,1,1);
    rbb_free(bb);
}

static void test_meta_repeated_source(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    check_meta(bb, 1,1,1);
    rbb_free(bb);
}

static void test_meta_in_place(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    check_meta(bb, 1,1,1);
    rbb_free(bb);
}

static void test_meta_read_after_write(void) {
    struct rbb_inst const inst[] = {
        {.op=IMM, .d=2, .imm=1},
        {.op=NEG, .d=0, .x=2},
    };
    struct rbb *bb = rbb(inst, 2);
    check_meta(bb, 0,1,3);
    rbb_free(bb);
}

static void test_meta_write_read_rewrite(void) {
    struct rbb_inst const inst[] = {
        {.op=IMM, .d=0, .imm=5},
        {.op=NEG, .d=1, .x=0},
        {.op=IMM, .d=0, .imm=10},
    };
    struct rbb *bb = rbb(inst, 3);
    check_meta(bb, 0,2,2);
    rbb_free(bb);
}

static void test_meta_CALL(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);
    check_meta(caller, 2,1,2);
    rbb_free(caller);
    rbb_free(callee);
}

static void test_jit_supported(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb_meta meta = rbb_meta(bb);
    meta.jit here;
    rbb_free(bb);
}

static void test_jit_unsupported(void) {
    // Programs that need more than MAX_JIT_REGS logical registers fall back to interp.
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=20, .x=20, .y=20}, 1);
    struct rbb_meta meta = rbb_meta(bb);
    !meta.jit here;
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
    test_NE();
    test_LT();
    test_LE();
    test_AND();
    test_OR();
    test_XOR();
    test_NOT();
    test_SEL();
    test_CALL();
    test_CALL_two_args();
    test_CALL_then_op();

    test_meta_empty();
    test_meta_no_inputs();
    test_meta_repeated_source();
    test_meta_in_place();
    test_meta_read_after_write();
    test_meta_write_read_rewrite();
    test_meta_CALL();

    test_jit_supported();
    test_jit_unsupported();
    return 0;
}
