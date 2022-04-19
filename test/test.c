#include "../lib/intro.h"

#include "basic.h"

#include "test.h"
#include "test.h.intro"

int
main(int argc, char ** argv) {
    printf("==== TEST OUTPUT ====\n\n");

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
    intro_print_struct(&obj, t_obj, NULL);
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
    intro_print_struct(&nest, ITYPE(Nest), NULL);
    printf("\n\n");

    /*=====================*/

    TestUndefined undef_test = {0};
    printf("undef_test = ");
    intro_print_struct(&undef_test, ITYPE(TestUndefined), NULL);
    printf("\n\n");

    /*====================*/

    TestDefault default_test;
    intro_set_defaults(&default_test, ITYPE(TestDefault));

    printf("default_test = ");
    intro_print_struct(&default_test, default_test.type, NULL);
    printf("\n");

    for (int i=0; i < LENGTH(default_test.words); i++) {
        printf("words[%i] = \"%s\";\n", i, default_test.words[i]);
    }

    return 0;
}
