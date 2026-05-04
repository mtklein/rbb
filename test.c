#include "rbb.h"
#include <stdint.h>
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

static const float    Tf = ((v8f)(v8i)-1).x;
static const _Float16 Th = ((v8h)(v8s)-1).x;

static _Bool exact_f(float x, float y) {
    union {
        float    f;
        uint32_t bits;
    } X={x}, Y={y};
    return X.bits == Y.bits;
}
static _Bool exact_h(_Float16 x, _Float16 y) {
    union {
        _Float16 h;
        uint16_t bits;
    } X={x}, Y={y};
    return X.bits == Y.bits;
}

static void test_IMM(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=3.14f}, 1);
    v8f reg[] = {0};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 3.14f) here;
    rbb_free(bb);
}

static void test_NEG(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    v8f reg[] = {42};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, -42) here;
    rbb_free(bb);
}

static void test_ABS(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ABS, .d=0, .x=0}, 1);
    v8f reg[] = {-42};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_SQRT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SQRT, .d=0, .x=0}, 1);
    v8f reg[] = {49};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 7) here;
    rbb_free(bb);
}

static void test_FLOOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FLOOR, .d=0, .x=0}, 1);
    v8f reg[] = {1.7f};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 1) here;
    rbb_free(bb);
}

static void test_CEIL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=CEIL, .d=0, .x=0}, 1);
    v8f reg[] = {1.3f};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 2) here;
    rbb_free(bb);
}

static void test_TRUNC(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=TRUNC, .d=0, .x=0}, 1);
    v8f reg[] = {-1.7f};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, -1) here;
    rbb_free(bb);
}

static void test_ROUND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ROUND, .d=0, .x=0}, 1);
    v8f reg[] = {1.5f};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 2) here;
    rbb_free(bb);
}

static void test_ADD(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);

    v8f reg[] = {42, 47};
    rbb_eval_f(bb, reg);

    exact_f(reg[0].x, 89) here;
    exact_f(reg[1].y, 47) here;

    rbb_free(bb);
}

static void test_SUB(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SUB, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {47, 42};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 5) here;
    rbb_free(bb);
}

static void test_MUL(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MUL, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {6, 7};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_DIV(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=DIV, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {84, 2};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 42) here;
    rbb_free(bb);
}

static void test_MIN(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MIN, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 3) here;
    rbb_free(bb);
}

static void test_MAX(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MAX, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 5) here;
    rbb_free(bb);
}

static void test_FMA(void) {
    // FMA d += x*y :  reg[0] = reg[0] + reg[1] * reg[2] = 4 + 2*3 = 10.
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FMA, .d=0, .x=1, .y=2}, 1);
    v8f reg[] = {4, 2, 3};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 10) here;
    rbb_free(bb);
}

static void test_EQ(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=EQ, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {42, 42};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, Tf) here;
    rbb_free(bb);
}

static void test_LT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LT, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {3, 5};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, Tf) here;
    rbb_free(bb);
}

static void test_LE(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LE, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {5, 5};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, Tf) here;
    rbb_free(bb);
}

static void test_AND(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=AND, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {Tf, 0};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 0) here;
    rbb_free(bb);
}

static void test_OR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=OR, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {Tf, 0};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, Tf) here;
    rbb_free(bb);
}

static void test_XOR(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=XOR, .d=0, .x=0, .y=1}, 1);
    v8f reg[] = {Tf, Tf};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 0) here;
    rbb_free(bb);
}

static void test_NOT(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NOT, .d=0, .x=0}, 1);
    v8f reg[] = {0};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, Tf) here;
    rbb_free(bb);
}

static void test_SEL(void) {
    // SEL d = d ? x : y :  reg[0] = reg[0] ? reg[1] : reg[2] = Tf ? 1 : 2 = 1.
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SEL, .d=0, .x=1, .y=2}, 1);
    v8f reg[] = {Tf, 1, 2};
    rbb_eval_f(bb, reg);
    exact_f(reg[0].x, 1) here;
    rbb_free(bb);
}

static void test_CALL(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8f reg[] = {21};
    rbb_eval_f(caller, reg);
    exact_f(reg[0].x, 42) here;

    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_two_args(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8f reg[] = {10, 32};
    rbb_eval_f(caller, reg);
    exact_f(reg[0].x, 42) here;

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
    rbb_eval_f(caller, reg);
    exact_f(reg[0].x, -42) here;

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

static void test_jit_f_supported(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb_meta meta = rbb_meta(bb);
    meta.jit_f here;
    rbb_free(bb);
}

static void test_IMM_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=0, .imm=3.5f}, 1);
    v8h reg[] = {0};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)3.5) here;
    rbb_free(bb);
}

static void test_NEG_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NEG, .d=0, .x=0}, 1);
    v8h reg[] = {42};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)-42) here;
    rbb_free(bb);
}

static void test_ABS_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ABS, .d=0, .x=0}, 1);
    v8h reg[] = {-42};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)42) here;
    rbb_free(bb);
}

static void test_SQRT_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SQRT, .d=0, .x=0}, 1);
    v8h reg[] = {49};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)7) here;
    rbb_free(bb);
}

static void test_FLOOR_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FLOOR, .d=0, .x=0}, 1);
    v8h reg[] = {(_Float16)1.75};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)1) here;
    rbb_free(bb);
}

static void test_CEIL_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=CEIL, .d=0, .x=0}, 1);
    v8h reg[] = {(_Float16)1.25};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)2) here;
    rbb_free(bb);
}

static void test_TRUNC_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=TRUNC, .d=0, .x=0}, 1);
    v8h reg[] = {(_Float16)-1.75};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)-1) here;
    rbb_free(bb);
}

static void test_ROUND_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ROUND, .d=0, .x=0}, 1);
    v8h reg[] = {(_Float16)1.5};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)2) here;
    rbb_free(bb);
}

static void test_ADD_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {42, 47};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)89) here;
    rbb_free(bb);
}

static void test_SUB_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SUB, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {47, 42};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)5) here;
    rbb_free(bb);
}

static void test_MUL_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MUL, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {6, 7};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)42) here;
    rbb_free(bb);
}

static void test_DIV_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=DIV, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {84, 2};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)42) here;
    rbb_free(bb);
}

static void test_MIN_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MIN, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {3, 5};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)3) here;
    rbb_free(bb);
}

static void test_MAX_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=MAX, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {3, 5};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)5) here;
    rbb_free(bb);
}

static void test_FMA_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=FMA, .d=0, .x=1, .y=2}, 1);
    v8h reg[] = {4, 2, 3};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)10) here;
    rbb_free(bb);
}

static void test_EQ_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=EQ, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {42, 42};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], Th) here;
    rbb_free(bb);
}

static void test_LT_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LT, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {3, 5};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], Th) here;
    rbb_free(bb);
}

static void test_LE_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=LE, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {5, 5};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], Th) here;
    rbb_free(bb);
}

static void test_AND_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=AND, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {Th, 0};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], 0) here;
    rbb_free(bb);
}

static void test_OR_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=OR, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {Th, 0};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], Th) here;
    rbb_free(bb);
}

static void test_XOR_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=XOR, .d=0, .x=0, .y=1}, 1);
    v8h reg[] = {Th, Th};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], 0) here;
    rbb_free(bb);
}

static void test_NOT_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=NOT, .d=0, .x=0}, 1);
    v8h reg[] = {0};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], Th) here;
    rbb_free(bb);
}

static void test_SEL_h(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=SEL, .d=0, .x=1, .y=2}, 1);
    v8h reg[] = {Th, 1, 2};
    rbb_eval_h(bb, reg);
    exact_h(reg[0][0], (_Float16)1) here;
    rbb_free(bb);
}

static void test_CALL_h(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8h reg[] = {21};
    rbb_eval_h(caller, reg);
    exact_h(reg[0][0], (_Float16)42) here;

    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_two_args_h(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);

    v8h reg[] = {10, 32};
    rbb_eval_h(caller, reg);
    exact_h(reg[0][0], (_Float16)42) here;

    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_then_op_h(void) {
    struct rbb *doubler = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller  = rbb((struct rbb_inst[]){
        {.op=CALL, .d=0, .call=doubler},
        {.op=NEG,  .d=0, .x=0},
    }, 2);

    v8h reg[] = {21};
    rbb_eval_h(caller, reg);
    exact_h(reg[0][0], (_Float16)-42) here;

    rbb_free(caller);
    rbb_free(doubler);
}

static void test_jit_h_supported(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb_meta meta = rbb_meta(bb);
    meta.jit_h here;
    rbb_free(bb);
}

static void test_jit_f_single_pump(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=20, .x=20, .y=20}, 1);
    struct rbb_meta meta = rbb_meta(bb);
    meta.jit_f here;
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

    test_IMM_h();
    test_NEG_h();
    test_ABS_h();
    test_SQRT_h();
    test_FLOOR_h();
    test_CEIL_h();
    test_TRUNC_h();
    test_ROUND_h();
    test_ADD_h();
    test_SUB_h();
    test_MUL_h();
    test_DIV_h();
    test_MIN_h();
    test_MAX_h();
    test_FMA_h();
    test_EQ_h();
    test_LT_h();
    test_LE_h();
    test_AND_h();
    test_OR_h();
    test_XOR_h();
    test_NOT_h();
    test_SEL_h();
    test_CALL_h();
    test_CALL_two_args_h();
    test_CALL_then_op_h();

    test_jit_f_supported();
    test_jit_h_supported();
    test_jit_f_single_pump();
    return 0;
}
