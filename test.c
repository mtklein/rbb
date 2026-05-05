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

static void test_CALL(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);
    { v8f reg[] = {21}; rbb_eval_f(caller, reg); exact_f(reg[0].x, 42) here; }
    { v8h reg[] = {21}; rbb_eval_h(caller, reg); exact_h(reg[0].x, 42) here; }
    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_two_args(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);
    { v8f reg[] = {10, 32}; rbb_eval_f(caller, reg); exact_f(reg[0].x, 42) here; }
    { v8h reg[] = {10, 32}; rbb_eval_h(caller, reg); exact_h(reg[0].x, 42) here; }
    rbb_free(caller);
    rbb_free(callee);
}

static void test_CALL_then_op(void) {
    struct rbb *doubler = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=0}, 1);
    struct rbb *caller  = rbb((struct rbb_inst[]){
        {.op=CALL, .d=0, .call=doubler},
        {.op=NEG,  .d=0, .x=0},
    }, 2);
    { v8f reg[] = {21}; rbb_eval_f(caller, reg); exact_f(reg[0].x, -42) here; }
    { v8h reg[] = {21}; rbb_eval_h(caller, reg); exact_h(reg[0].x, -42) here; }
    rbb_free(caller);
    rbb_free(doubler);
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

static void test_regs_CALL(void) {
    struct rbb *callee = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);
    struct rbb *caller = rbb(&(struct rbb_inst){.op=CALL, .d=0, .call=callee}, 1);
    rbb_regs(caller) == 2 here;
    rbb_free(caller);
    rbb_free(callee);
}

static void test_regs_opaque(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=IMM, .d=3, .imm=1.0f}, 1);
    rbb_regs(bb) == 4 here;
    rbb_free(bb);
}

static void test_roundtrip_565(void) {
    uint16_t src[] = {
        0x0000, 0xFFFF, 0x001F, 0x07E0,
        0xF800, 0x07FF, 0xFFFF, (16<<11)|(32<<5)|16,
    };
    struct cfg_load ld = load_565(src);
    struct cfg_store st = store_565(NULL, &ld.cfg);
    {
        uint16_t dst[8] = {0};
        st.dst = dst;
        v8f reg[3];
        cfg_eval_f(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
    {
        uint16_t dst[8] = {0};
        st.dst = dst;
        v8h reg[3];
        cfg_eval_h(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
}

static void test_roundtrip_8888(void) {
    uint32_t src[] = {
        0x00000000, 0xFFFFFFFF, 0x000000FF, 0x0000FF00,
        0x00FF0000, 0xFF000000, 0x80402010, 0xC0C0C0C0,
    };
    struct cfg_load ld = load_8888(src);
    struct cfg_store st = store_8888(NULL, &ld.cfg);
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8f reg[4];
        cfg_eval_f(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8h reg[4];
        cfg_eval_h(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
}

static void test_roundtrip_1010102(void) {
    uint32_t src[] = {
        0x00000000, 0xFFFFFFFF,
        0x000003FF, 0x000FFC00,
        0x3FF00000, 0xC0000000,
        (512) | (512 << 10) | (512 << 20) | (2u << 30),
        (100) | (200 << 10) | (300 << 20) | (1u << 30),
    };
    struct cfg_load ld = load_1010102(src);
    struct cfg_store st = store_1010102(NULL, &ld.cfg);
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8f reg[4];
        cfg_eval_f(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8h reg[4];
        cfg_eval_h(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
    }
}

static void test_roundtrip_fp16(void) {
    v4h src[8];
    for (int i = 0; i < 8; i++) {
        src[i] = (v4h){
            (_Float16)((float)(4*i+0) / 31.0f),
            (_Float16)((float)(4*i+1) / 31.0f),
            (_Float16)((float)(4*i+2) / 31.0f),
            (_Float16)((float)(4*i+3) / 31.0f),
        };
    }
    struct cfg_load ld = load_fp16(src);
    struct cfg_store st = store_fp16(NULL, &ld.cfg);
    {
        v4h dst[8] = {0};
        st.dst = dst;
        v8f reg[4];
        cfg_eval_f(&st.cfg, reg);
        for (int i = 0; i < 8; i++) {
            exact_h(dst[i][0], src[i][0]) here;
            exact_h(dst[i][1], src[i][1]) here;
            exact_h(dst[i][2], src[i][2]) here;
            exact_h(dst[i][3], src[i][3]) here;
        }
    }
    {
        v4h dst[8] = {0};
        st.dst = dst;
        v8h reg[4];
        cfg_eval_h(&st.cfg, reg);
        for (int i = 0; i < 8; i++) {
            exact_h(dst[i][0], src[i][0]) here;
            exact_h(dst[i][1], src[i][1]) here;
            exact_h(dst[i][2], src[i][2]) here;
            exact_h(dst[i][3], src[i][3]) here;
        }
    }
}

static void test_opaque(void) {
    uint32_t src[] = {
        0x00000000, 0x00FFFFFF, 0x000000FF, 0x0000FF00,
        0x00FF0000, 0x00010203, 0x80402010, 0x00C0C0C0,
    };
    struct rbb *opaque = rbb(&(struct rbb_inst){.op=IMM, .d=3, .imm=1.0f}, 1);
    struct cfg_load  ld  = load_8888(src);
    struct cfg_rbb   rbb = cfg_rbb(opaque, &ld.cfg);
    struct cfg_store st  = store_8888(NULL, &rbb.cfg);
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8f reg[4];
        cfg_eval_f(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == (src[i] | 0xFF000000) here; }
    }
    {
        uint32_t dst[8] = {0};
        st.dst = dst;
        v8h reg[4];
        cfg_eval_h(&st.cfg, reg);
        for (int i = 0; i < 8; i++) { dst[i] == (src[i] | 0xFF000000) here; }
    }
    rbb_free(opaque);
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
    test_CALL();
    test_CALL_two_args();
    test_CALL_then_op();

    test_regs_empty();
    test_regs_IMM();
    test_regs_repeated_source();
    test_regs_in_place();
    test_regs_gap();
    test_regs_CALL();
    test_regs_opaque();

    test_opaque();

    test_roundtrip_565();
    test_roundtrip_8888();
    test_roundtrip_1010102();
    test_roundtrip_fp16();
    return 0;
}
