#include "test.h"
#include "test.h.intro"

int
main(int argc, char ** argv) {
    TestAttributes obj = {0};
    obj.buffer_size = 8;
    obj.buffer = malloc(obj.buffer_size * sizeof(*obj.buffer));
    obj.v1 = 12345678;
    obj.h = -54321;
    for (int i=0; i < obj.buffer_size; i++) {
        obj.buffer[i] = i * i;
    }

    const IntroType * t_obj = intro_type_with_name("TestAttributes");
    assert(t_obj && t_obj->category == INTRO_STRUCT);

    printf("obj = ");
    intro_print(&obj, t_obj, NULL);
    printf("\n\n");

    /*=====================*/

    Nest nest = {
        .name = "Jon Garbuckle",
        .id = 32,
        .son = {
            .name = "Tim Garbuckle",
            .id = 33,
            .son = { .age = 5 },
        },
        .daughter = {
            .speed = 4.53,
        },

        .skills = SKILL_PROGRAMMER | SKILL_MUSICIAN | SKILL_BASEBALLBAT,
    };

    printf("nest = ");
    intro_print(&nest, ITYPE(Nest), NULL);
    printf("\n\n");

    /*=====================*/

    TestUndefined undef_test = {0};
    printf("undef_test = ");
    intro_print(&undef_test, ITYPE(TestUndefined), NULL);
    printf("\n\n");

    /*====================*/

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

    /*====================*/

    EvilEnum size_int = SIZEOF_INT;
    EvilEnum size_short = SIZEOF_SHORT;

    assert(size_int == sizeof(int));
    assert(size_short == sizeof(short));

    printf("size_int: %i\n", (int)size_int);
    printf("size_short: %i\n", (int)size_short);
    printf("\n");

    /*====================*/

    Dumb dumb;
    printf("dumb = ");
    intro_set_defaults(&dumb, ITYPE(Dumb));
    intro_print(&dumb, ITYPE(Dumb), NULL);
    printf("\n\n");

    for (int i=0; i < LENGTH(dumb.strange_array); i++) {
        assert(dumb.strange_array[i] == 0);
    }
#if 0 // @testfail anon_struct_array initialization
    for (int i=0; i < 4; i++) {
        assert(dumb.anon_struct_array[i].a == 3);
        assert(dumb.anon_struct_array[i].b == 4);
        assert(dumb.anon_struct_array[i].b == 5);
    }
#endif

    return 0;
}
