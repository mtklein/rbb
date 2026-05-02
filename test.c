#include "rbb.h"
#include <stdio.h>

#define here || (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap(), 0)

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

int main(void) {
    struct rbb foo = {
        2,6,7, {
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

    return 0;
}
