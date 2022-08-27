#include "test.h"

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

    const IntroType * t_obj = ITYPE(TestAttributes);
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

    bool it_showed_up = false;
    for (int i=INTRO_CTX->count_types - 1; i >= 0; i--) {
        const IntroType * type = &INTRO_CTX->types[i];
        if (type->name) {
            if (0==strcmp(type->name, "ThisShouldNotShowUp")) {
                assert(0 && "type was not ignored");
            }
            if (0==strcmp(type->name, "ThisShouldShowUp")) {
                it_showed_up = true;
            }
        }
    }

    assert(it_showed_up);

    return 0;
}
