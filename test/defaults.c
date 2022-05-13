#include "test.h"

int
main() {
    TestDefault default_test;
    intro_set_defaults(&default_test, ITYPE(TestDefault));

    printf("default_test = ");
    intro_print(&default_test, default_test.type, NULL);
    printf("\n");

    for (int i=0; i < LENGTH(default_test.words); i++) {
        printf("words[%i] = \"%s\";\n", i, default_test.words[i]);
    }
    printf("\n");

#define ABS(x) ((x)<0?-1 * (x):(x))
    assert(default_test.v_int == 123);
    assert(default_test.v_u8 == 1);
    assert(default_test.v_s64 == -54321);
    assert(ABS(default_test.v_float - 3.14159) < 0.0001);
    assert(default_test.type == ITYPE(TestDefault));
    assert(0==strcmp(default_test.name, "Brian"));
    assert(0==memcmp(default_test.numbers, &(uint8_t[8]){4, 5, 8, 9, 112, 9}, sizeof(default_test.numbers)));
    assert(0==strcmp(default_test.words[0], "apple"));
    assert(0==strcmp(default_test.words[1], "banana"));
    assert(0==strcmp(default_test.words[2], "mango"));
    assert(0==strcmp(default_test.words[3], "pineapple"));
    assert(0==strcmp(default_test.words[4], "newline\ntest"));
    float test_speeds [] = {3.4, 5.6, 1.7, 8.2, 0.002};
    assert(default_test.count_speeds == LENGTH(test_speeds));
    for (int i=0; i < default_test.count_speeds; i++) {
        assert(ABS(default_test.speeds[i] - test_speeds[i]) < 0.0001);
    }
    assert(default_test.skills == SKILL_PROGRAMMER | SKILL_MUSICIAN);

    return 0;
}
