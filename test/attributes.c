#include "basic.h"
#include <intro.h>
#include "../ext/stb_ds.h"

I(attribute my_ (
    joint: member,
    scale: float,
    num: int,
    friend: member,
    death_value: value(@inherit),
    exp: flag,
    handle: flag,
))

typedef struct {
    char * name I(length length_name, handle);
    int length_name;

    char * resume I(length length_resume, joint name);
    int length_resume;

    uint8_t * buffer I(length size_buffer, joint name);
    int size_buffer;

    size_t * sweet_nothings I(length count_sweet_nothings, joint name);
    int count_sweet_nothings;
} JointAllocTest;

I(gui_vector)
typedef struct {
    float x, y;
} Vector2;

typedef struct {
    I(note "this is the name")
    I(num 47)
    char * name I(9) I(fallback "spock");

    int v1 I(10, = 67, death_value -9, friend v2, my_scale 8.5, exp);

    I(id 11, exp, friend v1, death_value 2.5, note "i don't know what to put in here guys")
    float v2;

    Vector2 speed;
    Vector2 accel I(gui_scale 0.1);
    Vector2 internal_vec I(~gui_show, ~gui_vector);
} AttributeTest;

#include "attributes.c.intro"

void
special_alloc(void * dest_struct, const IntroType * type) {
    int owner_index = -1;
    for (int mi=0; mi < type->count; mi++) {
        const IntroMember * m = &type->members[mi];
        if (intro_has_attribute(m, my_handle)) {
            assert(m->type->category == INTRO_POINTER);
            owner_index = mi;

            struct _JoinInfo {int index; int ptr_offset;} stack [128];
            size_t stack_i = 0;
            size_t amount = 0;
            for (int mi=0; mi < type->count; mi++) {
                const IntroMember * m = &type->members[mi];
                int friend_index;
                if (
                    mi == owner_index
                    || (intro_attribute_member(m, my_joint, &friend_index)
                        && friend_index == owner_index)
                   )
                {
                    int element_size = m->type->of->size;
                    int64_t length;
                    IntroContainer parent = intro_container(dest_struct, type);
                    assert( intro_attribute_length(intro_push(&parent, mi), &length) );

                    struct _JoinInfo info;
                    info.index = mi;
                    info.ptr_offset = amount;
                    stack[stack_i++] = info;
                    amount += length * element_size;
                }
            }

            void * buffer = malloc(amount);
            assert(buffer);

            for (int i=0; i < stack_i; i++) {
                struct _JoinInfo info = stack[i];
                const IntroMember * m = &type->members[info.index];
                void ** member_loc = (void **)(dest_struct + m->offset);
                *member_loc = (void *)(buffer + info.ptr_offset);
            }
        }
    }
}

void
special_free(void * dest_struct, const IntroType * type) {
    for (int mi=0; mi < type->count; mi++) {
        const IntroMember * m = &type->members[mi];
        if (intro_has_attribute(m, my_handle)) {
            void * member_value = *(void **)(dest_struct + m->offset);
            free(member_value);
        }
    }
}

int
main() {
    JointAllocTest data = {0};

    data.length_name = 12;
    data.length_resume = 563;
    data.size_buffer = 1024;
    data.count_sweet_nothings = 3;

    special_alloc(&data, ITYPE(JointAllocTest));

    // TODO ...
    
    // TESTS
    {
        void * p = data.name;

        assert(data.name != NULL);
        p += data.length_name * sizeof(data.name[0]);

        assert(data.resume == p);
        p += data.length_resume * sizeof(data.resume[0]);

        assert(data.buffer == p);
        p += data.size_buffer * sizeof(data.buffer[0]);

        assert(data.sweet_nothings == p);
    }
    
    special_free(&data, ITYPE(JointAllocTest));

    AttributeTest test;
    intro_set_defaults(&test, ITYPE(AttributeTest));
    {
        int32_t i;
        float f;
        const char * note;

        const IntroMember *m_name = &ITYPE(AttributeTest)->members[0],
                          *m_v1   = &ITYPE(AttributeTest)->members[1],
                          *m_v2   = &ITYPE(AttributeTest)->members[2];

        IntroVariant var;
        assert(intro_attribute_value(m_name, gui_note, &var));
        note = (char *)var.data;
        assert(note);
        assert(0==strcmp(note, "this is the name"));

        assert(intro_attribute_int(m_name, my_num, &i));
        assert(i == 47);

        assert(0==strcmp(test.name, "spock"));

        assert(!intro_has_attribute(m_name, my_exp));

        assert(intro_attribute_int(m_name, id, &i) && i == 9);

        assert(test.v1 == 67);
        assert(intro_attribute_member(m_v1, my_friend, &i) && i == 2);
        assert(intro_attribute_float(m_v1, my_scale, &f) && f == 8.5);
        assert(intro_has_attribute(m_v1, my_exp));

        assert(test.v2 == 0.0f);
        assert(intro_has_attribute(m_v2, my_exp));
        assert(intro_attribute_member(m_v2, my_friend, &i) && i == 1);
        assert(intro_attribute_value(m_v2, gui_note, &var));
        note = (char *)var.data;
        assert(note);
        assert(0==strcmp(note, "i don't know what to put in here guys"));

        const IntroMember *m_speed = intro_member_by_name(ITYPE(AttributeTest), speed),
                          *m_accel = intro_member_by_name(ITYPE(AttributeTest), accel),
                          *m_internal_vec = intro_member_by_name(ITYPE(AttributeTest), internal_vec);

        assert(intro_has_attribute(ITYPE(Vector2), gui_vector));

        assert(intro_has_attribute(m_speed, gui_vector));
        assert(intro_has_attribute(m_accel, gui_scale));
        assert(intro_has_attribute(m_accel, gui_vector));
        assert(!intro_has_attribute(m_internal_vec, gui_show));
        assert(!intro_has_attribute(m_internal_vec, gui_vector));

        intro_set_values(&test, ITYPE(AttributeTest), my_death_value);
        assert(test.name == NULL);
        assert(test.v1 == -9);
        assert(test.v2 == 2.5);
    }

    return 0;
}
