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
} EnumTest;

typedef struct {
    struct {
        int hp I(when <-count_collected_ids >= 10);
    } stat; // 0

    uint16_t * collected_ids I(length count_collected_ids, when count_collected_ids > 0); // 1
    int count_collected_ids; // 2
    
    float speed; // 3
    bool ready I(when stat.hp >= 0); // 4

    bool test0 I(when .stat.hp * 15 - 3); // 5
} AttrTest;

#include "expression.c.intro"

int
main() {
    IntroEnumValue * e = ITYPE(EnumTest)->values;

    assert(e[0].value == E_0);
    assert(e[1].value == E_1);
    assert(e[2].value == E_2);
    assert(e[3].value == E_3);
    assert(e[4].value == E_4);
    assert(e[5].value == E_5);
    assert(e[6].value == E_6);

    AttrTest test = {0};
    test.stat.hp = 7;
    test.count_collected_ids = 2;

    int64_t value;
    IntroContainer cntr = intro_container(&test, ITYPE(AttrTest));
    IntroContainer stat_cntr = intro_push(&cntr, 0);
    assert(intro_attribute_expr_x(INTRO_CTX, intro_push(&cntr, 1),      IATTR_i_when, &value) && value == 1);
    assert(intro_attribute_expr_x(INTRO_CTX, intro_push(&cntr, 4),      IATTR_i_when, &value) && value == 1);
    assert(intro_attribute_expr_x(INTRO_CTX, intro_push(&stat_cntr, 0), IATTR_i_when, &value) && value == 0);
    assert(intro_attribute_expr_x(INTRO_CTX, intro_push(&cntr, 5),      IATTR_i_when, &value) && value == (test.stat.hp * 15 - 3));

    return 0;
}
