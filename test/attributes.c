#include "basic.h"
#include <intro.h>
#include "../ext/stb_ds.h"

typedef struct {uint32_t i;} SkpID;
typedef struct {float f;} SkpCity;

I(attribute my_ (
    joint: member,
    scale: float,
    num: int,
    friend: member,
    death_value: value(@inherit),
    exp: flag,
    handle: flag,

    gene_num: int @propagate,
    non_gene_num: int,
    comp_only: flag @transient,
))

I(attribute Skp (
    id: value(SkpID),
    city: value(SkpCity),
    num: int,
    Header: type,
    Array:  flag @transient @imply(Header SkpArrayHeader),
))

I(attribute @global (
    num: float,
))

typedef struct {
    size_t len;
    size_t cap;
} SkpArrayHeader;

typedef struct {
    char * name I(length length_name, my_handle);
    int length_name;

    char * resume I(length length_resume, my_joint name);
    int length_resume;

    uint8_t * buffer I(length size_buffer, my_joint name);
    int size_buffer;

    size_t * sweet_nothings I(length count_sweet_nothings, my_joint name);
    int count_sweet_nothings;
} JointAllocTest;

I(gui: vector, note "the grungl",
  my: gene_num -83, non_gene_num 578)
typedef struct {
    float x, y;
} Vector2;

typedef Vector2 SpecialVec2;

typedef struct {
    I(gui_note "this is the name")
    I(my_num 47)
    char * name I(9) I(fallback "spock");

    int v1 I(10, = 67, my: death_value -9, friend v2, scale 8.5, exp);

    I(id 11,
        my: exp, friend v1, death_value 2.5,
        gui: note "i don't know what to put in here guys")
    float v2;

    Vector2 speed;
    Vector2 accel I(gui_scale 0.1);
    Vector2 internal_vec I(gui: ~show, ~vector);

    SpecialVec2 special_vec1;
    SpecialVec2 special_vec2 I(my_: non_gene_num 612, comp_only);

    I(
        ~city,      // start in global namespace
        my:  num 5, // specified namespace takes precedence over global namespace
        Skp: num -5, id {4}, city {4.5}, alias large_test, // the global namespace can still be used if a name isn't shadowed
        @global: num 5.0, id 5, // clear specified namespace
     )
    int big_test;

    int * some_nums I(SkpArray);
} AttributeTest;

typedef enum {
    SKP_FIRST = 0 I(alias first),
    SKP_SECOND I(alias second),
    I(alias third) SKP_THIRD,
} SkpEnum;

typedef struct {
    int a : 3;
    int b : 1;
    int c : 10;
} BitfieldTest;

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
                    IntroContainer parent = intro_cntr(dest_struct, type);
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
    intro_fallback(&test, ITYPE(AttributeTest));
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

        const IntroMember *m_speed =        intro_member_by_name(ITYPE(AttributeTest), speed)
                        , *m_accel =        intro_member_by_name(ITYPE(AttributeTest), accel)
                        , *m_internal_vec = intro_member_by_name(ITYPE(AttributeTest), internal_vec)
                        , *m_special_vec1 = intro_member_by_name(ITYPE(AttributeTest), special_vec1)
                        , *m_special_vec2 = intro_member_by_name(ITYPE(AttributeTest), special_vec2)
                        , *m_big_test =     intro_member_by_name(ITYPE(AttributeTest), big_test)
                        ;

        assert(intro_has_attribute(ITYPE(Vector2), gui_vector));

        assert(intro_has_attribute(m_accel, gui_scale));
        assert(!intro_has_attribute(m_internal_vec, gui_show));
        assert(!intro_has_attribute(m_internal_vec, gui_vector));

        assert(!intro_has_attribute(m_special_vec2, my_comp_only));

        // PROPAGATION TESTS
        {
            int32_t val;

            assert(intro_has_attribute(m_speed, gui_vector));
            assert(intro_has_attribute(m_accel, gui_vector));
            assert(intro_attribute_int(m_accel, my_gene_num, &val) && val == -83);
            assert(!intro_has_attribute(m_accel, my_non_gene_num));

            val = 0;
            assert(intro_has_attribute(ITYPE(SpecialVec2), gui_vector));
            assert(intro_attribute_int(ITYPE(SpecialVec2), my_gene_num, &val) && val == -83);
            assert(!intro_has_attribute(ITYPE(SpecialVec2), my_non_gene_num));

            val = 0;
            assert(intro_has_attribute(m_special_vec1, gui_vector));
            assert(intro_attribute_int(m_special_vec1, my_gene_num, &val) && val == -83);

            val = 0;
            assert(intro_has_attribute(m_special_vec2, gui_vector));
            assert(intro_attribute_int(m_special_vec2, my_gene_num, &val) && val == -83);
            assert(intro_attribute_int(m_special_vec2, my_non_gene_num, &val) && val == 612);

            intro_set_value(&test, ITYPE(AttributeTest), my_death_value);
            assert(test.name == NULL);
            assert(test.v1 == -9);
            assert(test.v2 == 2.5);
        }

        // big_test
        {
            float fval;
            int32_t ival;
            IntroVariant var;

            assert(!intro_has_attribute(m_big_test, city));
            assert(intro_attribute_int(m_big_test, my_num, &ival)   && ival == 5);
            assert(intro_attribute_int(m_big_test, Skpnum, &ival)   && ival == -5);
            assert(intro_attribute_value(m_big_test, Skpid, &var)   && intro_var_get(var, SkpID).i == 4);
            assert(intro_attribute_value(m_big_test, Skpcity, &var) && intro_var_get(var, SkpCity).f == 4.5);
            assert(intro_attribute_value(m_big_test, alias, &var)   && 0==strcmp((char *)var.data, "large_test"));
            assert(intro_attribute_float(m_big_test, num, &fval)    && fval == 5.0);
            assert(intro_attribute_int(m_big_test, id, &ival)       && ival == 5);
        }
    }

    // enum test
    {
        IntroVariant var;

        const IntroEnumValue * e = ITYPE(SkpEnum)->values;
        assert(intro_attribute_value_x(INTRO_CTX, NULL, e[0].attr, IATTR_alias, &var) && 0==strcmp((char *)var.data, "first"));
        assert(intro_attribute_value_x(INTRO_CTX, NULL, e[1].attr, IATTR_alias, &var) && 0==strcmp((char *)var.data, "second"));
        assert(intro_attribute_value_x(INTRO_CTX, NULL, e[2].attr, IATTR_alias, &var) && 0==strcmp((char *)var.data, "third"));
    }

    // bitfield test
    {
        int32_t field;

        const IntroMember *m_a = intro_member_by_name(ITYPE(BitfieldTest), a)
                        , *m_b = intro_member_by_name(ITYPE(BitfieldTest), b)
                        , *m_c = intro_member_by_name(ITYPE(BitfieldTest), c)
                        ;

        assert(intro_attribute_int(m_a, bitfield, &field) && field == 3);
        assert(intro_attribute_int(m_b, bitfield, &field) && field == 1);
        assert(intro_attribute_int(m_c, bitfield, &field) && field == 10);
    }

    IntroVariant var;
    assert(intro_attribute_value_x(INTRO_CTX, NULL, ITYPE(Vector2)->attr, IATTR_gui_note, &var) && 0==strcmp((char *)var.data, "the grungl"));

    // type test
    {
        assert(intro_attribute_type(intro_member_by_name(ITYPE(IntroType), flags), imitate) == ITYPE(IntroFlags));
        assert(intro_attribute_type(intro_member_by_name(ITYPE(IntroType), category), imitate) == ITYPE(IntroCategory));
    }

    // imply test
    {
        const IntroMember *m_some_nums = intro_member_by_name(ITYPE(AttributeTest), some_nums);
        assert(!intro_has_attribute(m_some_nums, SkpArray));
        assert(intro_attribute_type(m_some_nums, SkpHeader) == ITYPE(SkpArrayHeader));
    }

    return 0;
}
