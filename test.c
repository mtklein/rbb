#include "rbb.h"
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static void test_add(void) {
    struct rbb *bb = rbb(&(struct rbb_inst){.op=ADD, .d=0, .x=0, .y=1}, 1);

    v8f reg[] = {42, 47};
    rbb_eval(bb, reg);

    equiv(reg[0].x, 89) here;
    equiv(reg[1].y, 47) here;

    rbb_free(bb);
}

int main(void) {
    test_add();
    return 0;
}
