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
