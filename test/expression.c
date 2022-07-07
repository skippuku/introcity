#include <intro.h>
#include "basic.h"

typedef struct {
    char a;
    short b;
} TestSize;

typedef enum {
    E_0 = 4 + 5 / 2, // 6
    E_1 = sizeof(int), // 4
    E_2 = sizeof(int) * 4, // 16
    E_3 = (sizeof(void *) - 4) / 2, // 2
    E_4 = (sizeof(int16_t) == 2)? 6 % 5 : 0x0f & 0x5, // 1
    E_5 = (sizeof(int32_t) == 2)? 6 % 5 : 0x0f & 0x25, // 0x5

    E_6 = sizeof(TestSize) * 2 + 4,
} Test;

#include "expression.c.intro"

int
main() {
    IntroEnumValue * e = ITYPE(Test)->values;

    assert(e[0].value == E_0);
    assert(e[1].value == E_1);
    assert(e[2].value == E_2);
    assert(e[3].value == E_3);
    assert(e[4].value == E_4);
    assert(e[5].value == E_5);
    assert(e[6].value == E_6);

    return 0;
}
