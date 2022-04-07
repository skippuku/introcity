#include "../lib/lib.c"

#include "basic.h"

#include "test.h"
#include "test.h.intro"

static IntroType *
get_type_with_name(const char * name) {
    for (int i=0; i < LENGTH(__intro_types); i++) {
        IntroType * type = &__intro_types[i];
        if (type->name && strcmp(type->name, name) == 0) {
            return type;
        }
    }
    return NULL;
}

void
set_defaults(void * dest, const IntroType * type) {
    for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
        const IntroMember * m = &type->i_struct->members[m_index];
        size_t size = intro_size(m->type);
        int32_t offset;
        if (intro_attribute_int(m, INTRO_ATTR_DEFAULT, &offset)) {
            const void * value = __intro_values + offset;
            memcpy(dest + m->offset, value, size);
        } else if (intro_attribute_flag(m, INTRO_ATTR_TYPE)) {
            memcpy(dest + m->offset, &type, sizeof(void *));
        } else {
            memset(dest + m->offset, 0, size);
        }
    }
}

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

    const IntroType * t_obj = get_type_with_name("TestAttributes");
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
    intro_print_struct(&nest, get_type_with_name("Nest"), NULL);
    printf("\n\n");

    /*=====================*/

    TestUndefined undef_test = {0};
    printf("undef_test = ");
    intro_print_struct(&undef_test, get_type_with_name("TestUndefined"), NULL);
    printf("\n\n");

    /*====================*/

    TestDefault default_test;
    set_defaults(&default_test, get_type_with_name("TestDefault"));

    printf("default_test = ");
    intro_print_struct(&default_test, default_test.type, NULL);
    printf("\n");

    return 0;
}
