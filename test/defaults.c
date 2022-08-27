#include "test.h"

#define ABS(x) (((x) < 0)? (x) * -1 : (x))

int
main() {
    TestDefault default_test;
    intro_fallback(&default_test, ITYPE(TestDefault));

    assert(default_test.v_int == 123);
    assert(default_test.v_u8 == 1);
    assert(default_test.v_s64 == -54321);
    assert(ABS(default_test.v_float - 3.14159) < 0.0001);
    assert(default_test.type == ITYPE(TestDefault));
    assert(0==strcmp(default_test.name, "Brian"));
    assert(0==memcmp(default_test.numbers, &(uint8_t[8]){4, 5, 8, 9, 112, 9}, sizeof(default_test.numbers)));
    assert(0==strcmp(default_test.words[3], "apple"));
    assert(0==strcmp(default_test.words[1], "banana"));
    assert(0==strcmp(default_test.words[2], "mango"));
    assert(0==strcmp(default_test.words[0], "pineapple"));
    assert(0==strcmp(default_test.words[4], "newline\ntest"));
    float test_speeds [] = {3.4, 5.6, 1.7, 8.2, 0.002};
    assert(default_test.count_speeds == LENGTH(test_speeds));
    for (int i=0; i < default_test.count_speeds; i++) {
        assert(ABS(default_test.speeds[i] - test_speeds[i]) < 0.0001);
    }
    assert(default_test.skills == (SKILL_PROGRAMMER | SKILL_MUSICIAN));

    assert(ABS(default_test.v3.x - 3.5) < 0.0001);
    assert(ABS(default_test.v3.y - -0.75) < 0.0001);
    assert(ABS(default_test.v3.z - 12.25) < 0.0001);

    assert(default_test.align.a == 15);
    assert(default_test.align.b == 2001);
    assert(ABS(default_test.align.c - 100456.12) < 0.000001);

    // Nest

    Nest nest;
    intro_fallback(&nest, ITYPE(Nest));

    printf("nest = ");
    intro_print(&nest, ITYPE(Nest), NULL);
    printf("\n");

    assert(nest.id == 5);
    assert(nest.son.id == 6);
    assert(nest.son.son.age == 4);
    assert(nest.son.son.favorite_fruit == FRUIT_PEACH);
    assert(nest.daughter.speed == 7.5);
    assert(0==strcmp(nest.name, "Emerald Power"));
    assert(0==strcmp(nest.son.name, "Kyle"));

    // Dumb

    Dumb dumb;
    memset(&dumb, 0xff, sizeof(dumb));

    intro_fallback(&dumb, ITYPE(Dumb));
    printf("dumb = ");
    intro_print(&dumb, ITYPE(Dumb), NULL);
    printf("\n\n");

    const IntroType * strange_array_type = ITYPE(Dumb)->members[0].type;
    assert(strange_array_type->category == INTRO_ARRAY);
    assert(strange_array_type->count == LENGTH(dumb.strange_array));
    for (int i=0; i < LENGTH(dumb.strange_array); i++) {
        assert(dumb.strange_array[i] == 0);
    }

    for (int i=0; i < 4; i++) {
        assert(dumb.anon_struct_array[i].a == 3);
        assert(dumb.anon_struct_array[i].b == 4);
        assert(dumb.anon_struct_array[i].u == 5);
    }

    return 0;
}
