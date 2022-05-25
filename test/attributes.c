#include "basic.h"
#include <intro.h>
#include <ext/stb_ds.h>

typedef enum I(attribute) {
    ATTR_JOINT I(member joint),
    ATTR_HANDLE I(flag handle),

    ATTR_SCALE I(float scale),
    ATTR_NUM I(int num),
    ATTR_FRIEND I(member friend),
    ATTR_DEATH_VALUE I(value death_value),
    ATTR_EXP I(flag exp),
} CustomAttributes;

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

typedef struct {
    char * name I(note "this is the name", num 47, default "spock", 9);
    int v1 I(10, = 67, death_value -9, friend v2, scale 8.5, exp);
    float v2 I(id 11, exp, friend v1, death_value 2.5, note "i don't know what to put in here guys");
} AttributeTest;

#include "attributes.c.intro"

void
special_alloc(void * dest_struct, const IntroType * type) {
    int owner_index = -1;
    for (int mi=0; mi < type->i_struct->count_members; mi++) {
        const IntroMember * m = &type->i_struct->members[mi];
        if (intro_attribute_flag(m, ATTR_HANDLE)) {
            assert(m->type->category == INTRO_POINTER);
            owner_index = mi;

            struct join_info {int index; int ptr_offset;} * stack = NULL;
            size_t amount = 0;
            for (int mi=0; mi < type->i_struct->count_members; mi++) {
                const IntroMember * m = &type->i_struct->members[mi];
                int friend_index;
                if (mi == owner_index
                    || (intro_attribute_int(m, ATTR_JOINT, &friend_index)
                        && friend_index == owner_index))
                {
                    int element_size = intro_size(m->type->parent);
                    int64_t length;
                    assert( intro_attribute_length(dest_struct, type, m, &length) );

                    struct join_info info;
                    info.index = mi;
                    info.ptr_offset = amount;
                    arrput(stack, info);
                    amount += length * element_size;
                }
            }

            void * buffer = malloc(amount);
            assert(buffer);

            for (int i=0; i < arrlen(stack); i++) {
                struct join_info info = stack[i];
                const IntroMember * m = &type->i_struct->members[info.index];
                void ** member_loc = (void **)(dest_struct + m->offset);
                *member_loc = (void *)(buffer + info.ptr_offset);
            }

            arrfree(stack);
        }
    }
}

void
special_free(void * dest_struct, const IntroType * type) {
    for (int mi=0; mi < type->i_struct->count_members; mi++) {
        const IntroMember * m = &type->i_struct->members[mi];
        if (intro_attribute_flag(m, ATTR_HANDLE)) {
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

        const IntroMember *m_name = &ITYPE(AttributeTest)->i_struct->members[0],
                          *m_v1   = &ITYPE(AttributeTest)->i_struct->members[1],
                          *m_v2   = &ITYPE(AttributeTest)->i_struct->members[2];

        assert(intro_attribute_int(m_name, INTRO_ATTR_NOTE, &i));
        assert(0==strcmp(__intro_notes[i], "this is the name"));

        assert(intro_attribute_int(m_name, ATTR_NUM, &i) && i == 47);

        assert(0==strcmp(test.name, "spock"));

        assert(!intro_attribute_flag(m_name, ATTR_EXP));

        assert(intro_attribute_int(m_name, INTRO_ATTR_ID, &i) && i == 9);
        assert(m_name->id == 9);

        assert(test.v1 == 67);
        assert(m_v1->id == 10);
        assert(intro_attribute_int(m_v1, ATTR_FRIEND, &i) && i == 2);
        assert(intro_attribute_float(m_v1, ATTR_SCALE, &f) && f == 8.5);
        assert(intro_attribute_flag(m_v1, ATTR_EXP));

        assert(test.v2 == 0.0f);
        assert(m_v2->id == 11);
        assert(intro_attribute_flag(m_v2, ATTR_EXP));
        assert(intro_attribute_int(m_v2, ATTR_FRIEND, &i) && i == 1);
        assert(intro_attribute_int(m_v2, INTRO_ATTR_NOTE, &i));
        assert(0==strcmp(__intro_notes[i], "i don't know what to put in here guys"));

        intro_set_values(&test, ITYPE(AttributeTest), ATTR_DEATH_VALUE);
        assert(test.name == NULL);
        assert(test.v1 == -9);
        assert(test.v2 == 2.5);
    }

    return 0;
}
