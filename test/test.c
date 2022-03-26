#include "basic.h"

#include "../intro.h"

#include "test.h"
#include "test.h.intro"

#define INTRO_IS_NUMBER(type) (type->category >= INTRO_U8 && type->category <= INTRO_F32)

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
print_number(void * data, const IntroMember * m) {
    switch(m->type->category) {
    case INTRO_U8:
        printf("%hhu", *(uint8_t *)(data + m->offset));
        break;
    case INTRO_U16:
        printf("%hu",  *(uint16_t *)(data + m->offset));
        break;
    case INTRO_U32:
        printf("%u",   *(uint32_t *)(data + m->offset));
        break;
    case INTRO_U64:
        printf("%lu", *(uint64_t *)(data + m->offset));
        break;

    case INTRO_S8:
        printf("%hhi", *(int8_t *)(data + m->offset));
        break;
    case INTRO_S16:
        printf("%hi",  *(int16_t *)(data + m->offset));
        break;
    case INTRO_S32:
        printf("%i",   *(int32_t *)(data + m->offset));
        break;
    case INTRO_S64:
        printf("%li", *(int64_t *)(data + m->offset));
        break;

    case INTRO_F32:
        printf("%f", *(float *)(data + m->offset));
        break;
    case INTRO_F64:
        printf("%f", *(double *)(data + m->offset));
        break;

    default:
        printf("NaN");
        break;
    }
}

bool
intro_attribute_int(IntroMember * m, int32_t attr_type, int32_t * o_int) {
    for (int i=0; i < m->count_attributes; i++) {
        const IntroAttributeData * attr = &m->attributes[i];
        if (attr->type == attr_type) {
            if (o_int) *o_int = attr->v.i;
            return true;
        }
    }
    return false;
}

bool
intro_attribute_member_index(IntroMember * m, int32_t attr_type, int32_t * o_index) {
    for (int i=0; i < m->count_attributes; i++) {
        const IntroAttributeData * attr = &m->attributes[i];
        if (attr->type == attr_type) {
            if (o_index) *o_index = attr->v.i;
            return true;
        }
    }
    return false;
}

int
main(int argc, char ** argv) {
    printf("test compiled.\n");

    TestAttributes obj = {0};
    obj.buffer_size = 8;
    obj.buffer = malloc(obj.buffer_size * sizeof(*obj.buffer));
    obj.v1 = 12345678;
    obj.h = -54321;
    void * obj_data = &obj;

    const IntroType * t_obj = get_type_with_name("TestAttributes");
    assert(t_obj && t_obj->category == INTRO_STRUCT);
    printf("%s obj = {\n", t_obj->name);
    for (int m_index = 0; m_index < t_obj->i_struct->count_members; m_index++) {
        IntroMember * m = &t_obj->i_struct->members[m_index];
        printf("  (%s) .%s = ", m->type->name ?: "*", m->name);

        s32 length_member_index;
        if (INTRO_IS_NUMBER(m->type)) {
            print_number(&obj, m);
        } else if (m->type->category == INTRO_POINTER 
                && intro_attribute_member_index(m, INTRO_ATTR_LENGTH, &length_member_index)) {
            IntroMember * length_member = &t_obj->i_struct->members[length_member_index];

            assert(length_member->type->category == INTRO_S32);
            int32_t length = *(int32_t *)(obj_data + length_member->offset);

            assert(m->type->parent->category == INTRO_U8);
            u8 * array = *(u8 **)(obj_data + m->offset);
            printf("{");
            for (int i=0; i < length; i++) {
                if (i > 0) printf(", ");
                printf("%hhu", array[i]);
            }
            printf("}");
        }
        printf(";");
        int32_t id;
        if (intro_attribute_int(m, INTRO_ATTR_ID, &id)) {
            printf(" // id(%i)", id);
        }
        printf("\n");
    }
    printf("};\n");

    return 0;
}
