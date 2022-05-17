#include <intro.h>
#include "basic.h"

typedef enum {
    E_1 = 4 + 5 / 2, // 6
    E_2 = sizeof(int), // 4
    E_3 = sizeof(int) * 4, // 16
    E_4 = (sizeof(void *) - 4) / 2, // 2
    E_5 = (sizeof(int16_t) == 2)? 6 % 5 : 0x0f & 0x5, // 1
    E_6 = (sizeof(int32_t) == 2)? 6 % 5 : 0x0f & 0x25, // 0x5
} Test;

#include "expression.c.intro"

int
main() {
    assert(E_1 == 6);
    assert(E_2 == 4);
    assert(E_3 == 16);
    assert(E_4 == 2);
    assert(E_5 == 1);
    assert(E_6 == 0x5);

    return 0;
}
