#include "../intro.h"

#include "../util.c"

#ifndef INTRO_CTX
#define INTRO_CTX &__intro_ctx
#endif
#define intro_type_with_name(name) intro_type_with_name_ctx(INTRO_CTX, name)
#define intro_set_defaults(dest, type) intro_set_defaults_ctx(INTRO_CTX, dest, type)

bool
intro_is_scalar(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

bool
intro_is_int(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_S64);
}

static bool
intro_is_complex(const IntroType * type) {
    return (type->category == INTRO_STRUCT
         || type->category == INTRO_UNION
         || type->category == INTRO_ENUM);
}

int
intro_size(const IntroType * type) {
    if (intro_is_scalar(type)) {
        return (type->category & 0x0f);
    } else if (type->category == INTRO_POINTER) {
        return sizeof(void *);
    } else if (type->category == INTRO_ARRAY) {
        return type->array_size * intro_size(type->parent);
    } else if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        return type->i_struct->size;
    } else if (type->category == INTRO_ENUM) {
        return type->i_enum->size;
    } else {
        return 0;
    }
}

const IntroType *
intro_base(const IntroType * type, int * o_depth) {
    int depth = 0;
    while (type->category == INTRO_ARRAY || type->category == INTRO_POINTER) {
        type = type->parent;
        depth++;
    }

    if (o_depth) *o_depth = depth;
    return type;
}

int64_t
intro_int_value(const void * data, const IntroType * type) {
    int64_t result;
    switch(type->category) {
    case INTRO_U8:
        result = *(uint8_t *)data;
        break;
    case INTRO_U16:
        result = *(uint16_t *)data;
        break;
    case INTRO_U32:
        result = *(uint32_t *)data;
        break;
    case INTRO_U64: {
        uint64_t u_result = *(uint64_t *)data;
        result = (u_result <= INT64_MAX)? u_result : INT64_MAX;
        break;
    }

    case INTRO_S8:
        result = *(int8_t *)data;
        break;
    case INTRO_S16:
        result = *(int16_t *)data;
        break;
    case INTRO_S32:
        result = *(int32_t *)data;
        break;
    case INTRO_S64:
        result = *(int64_t *)data;
        break;

    default:
        result = 0;
        break;
    }

    return result;
}

bool
intro_attribute_flag(const IntroMember * m, int32_t attr_type) {
    for (int i=0; i < m->count_attributes; i++) {
        const IntroAttributeData * attr = &m->attributes[i];
        if (attr->type == attr_type) {
            return true;
        }
    }
    return false;
}

bool
intro_attribute_int(const IntroMember * m, int32_t attr_type, int32_t * o_int) {
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
intro_attribute_float(const IntroMember * m, int32_t attr_type, float * o_float) {
    for (int i=0; i < m->count_attributes; i++) {
        const IntroAttributeData * attr = &m->attributes[i];
        if (attr->type == attr_type) {
            if (o_float) *o_float = attr->v.f;
            return true;
        }
    }
    return false;
}

bool
intro_attribute_length(const void * struct_data, const IntroType * struct_type, const IntroMember * m, int64_t * o_length) {
    int32_t member_index;
    const void * m_data = struct_data + m->offset;
    if (intro_attribute_int(m, INTRO_ATTR_LENGTH, &member_index)) {
        const IntroMember * length_member = &struct_type->i_struct->members[member_index];
        const void * length_member_loc = struct_data + length_member->offset;
        *o_length = intro_int_value(length_member_loc, length_member->type);
        return true;
    } else {
        *o_length = 0;
        return false;
    }
}

void
intro_offset_pointers(void * dest, const IntroType * type, void * base) {
    if (type->category == INTRO_ARRAY) {
        if (type->parent->category == INTRO_POINTER) {
            for (int i=0; i < type->array_size; i++) {
                void ** o_ptr = (void **)(dest + i * sizeof(void *));
                *o_ptr += (size_t)base;
            }
        }
    }
}

void
intro_set_member_value_ctx(IntroContext * ctx, void * dest, const IntroType * struct_type, int member_index, int value_attribute) {
    const IntroMember * m = &struct_type->i_struct->members[member_index];
    size_t size = intro_size(m->type);
    int32_t value_offset;
    if (intro_attribute_int(m, value_attribute, &value_offset)) {
        void * value_ptr = ctx->values + value_offset;
        if (m->type->category == INTRO_POINTER) {
            size_t data_offset = *(size_t *)value_ptr;
            void * data = ctx->values + data_offset;
            int32_t data_length = *(int32_t *)(data - 4);
            memcpy(dest + m->offset, &data, sizeof(size_t));
            int32_t length_member_index;
            if (intro_attribute_int(m, INTRO_ATTR_LENGTH, &length_member_index)) {
                const IntroMember * length_member = &struct_type->i_struct->members[length_member_index];
                size_t length_member_size = intro_size(length_member->type);
                memcpy(dest + length_member->offset, &data_length, length_member_size);
            }
        } else {
            memcpy(dest + m->offset, value_ptr, size);
            intro_offset_pointers(dest + m->offset, m->type, ctx->values);
        }
    } else if (intro_attribute_flag(m, INTRO_ATTR_TYPE)) {
        memcpy(dest + m->offset, &struct_type, sizeof(void *));
    } else {
        memset(dest + m->offset, 0, size);
    }
}

void
intro_set_values_ctx(IntroContext * ctx, void * dest, const IntroType * type, int value_attribute) {
    for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
        intro_set_member_value_ctx(ctx, dest, type, m_index, value_attribute);
    }
}

void
intro_set_defaults_ctx(IntroContext * ctx, void * dest, const IntroType * type) {
    intro_set_values_ctx(ctx, dest, type, INTRO_ATTR_DEFAULT);
}

typedef struct IntroNameSize {
    char * name;
    size_t size;
} IntroNameSize;

void *
intro_joint_alloc(void * dest, const IntroType * type, const IntroNameSize * list, size_t count) {
    int32_t member_indices [count];
    size_t ptr_offsets [count];
    size_t alloc_size = 0;
    for (int i=0; i < count; i++) {
        for (int mi=0; mi < type->i_struct->count_members; mi++) { // NOTE: slow search
            const IntroMember * m = &type->i_struct->members[mi];
            if (0==strcmp(m->name, list[i].name)) {
                assert(m->type->category == INTRO_POINTER);
                member_indices[i] = mi;
                ptr_offsets[i] = alloc_size;
                alloc_size += list[i].size;
                break;
            }
        }
    }

    void * buffer = malloc(alloc_size);
    if (!buffer) return NULL;

    for (int i=0; i < count; i++) {
        int mi = member_indices[i];
        const IntroMember * m = &type->i_struct->members[mi];
        void ** member_loc = (void **)(dest + m->offset);
        *member_loc = (void *)(buffer + ptr_offsets[i]);
    }

    return buffer;
}

void
intro_print_basic(const void * data, const IntroType * type) {
    if (intro_is_scalar(type)) {
        if (type->category <= INTRO_S64) {
            int64_t value = intro_int_value(data, type);
            printf("%li", value);
        } else if (type->category == INTRO_F32) {
            printf("%f", *(float *)data);
        } else if (type->category == INTRO_F64) {
            printf("%f", *(double *)data);
        } else {
            printf("<unknown>");
        }
    } else {
        printf("<NaN>");
    }
}

void
intro_print_enum(int value, const IntroType * type) {
    const IntroEnum * i_enum = type->i_enum;
    if (i_enum->is_sequential) {
        if (value >= 0 && value < i_enum->count_members) {
            printf("%s", i_enum->members[value].name);
        } else {
            printf("%i", value);
        }
    } else if (i_enum->is_flags) {
        bool more_than_one = false;
        if (value) {
            for (int f=0; f < i_enum->count_members; f++) {
                if (value & i_enum->members[f].value) {
                    if (more_than_one) printf(" | ");
                    printf("%s", i_enum->members[f].name);
                    more_than_one = true;
                }
            }
        } else {
            printf("0");
        }
    } else {
        printf("%i", value);
    }
}

void
intro_print_basic_array(const void * data, const IntroType * type, int length) {
    int elem_size = intro_size(type);
    if (elem_size) {
        printf("{");
        for (int i=0; i < length; i++) {
            if (i > 0) printf(", ");
            intro_print_basic(data + elem_size * i, type);
        }
        printf("}");
    } else {
        printf("<concealed>");
    }
}

void
intro_sprint_type_name(char * dest, const IntroType * type) {
    while (1) {
        if (type->category == INTRO_POINTER) {
            *dest++ = '*';
            type = type->parent;
        } else if (type->category == INTRO_ARRAY) {
            dest += 1 + stbsp_sprintf(dest, "[%u]", type->array_size) - 1;
            type = type->parent;
        } else if (type->name) {
            dest += 1 + stbsp_sprintf(dest, "%s", type->name) - 1;
            break;
        } else {
            dest += 1 + stbsp_sprintf(dest, "<anon>");
            break;
        }
    }
    *dest = '\0';
}

void
intro_print_type_name(const IntroType * type) {
    char buf [1024];
    intro_sprint_type_name(buf, type);
    fputs(buf, stdout);
}

typedef struct {
    int indent;
} IntroPrintOptions;

void
intro_print_struct(const void * data, const IntroType * type, const IntroPrintOptions * opt) {
    static const IntroPrintOptions opt_default = {0};
    static const char * tab = "    ";

    if (type->category != INTRO_STRUCT && type->category != INTRO_UNION) {
        return;
    }

    if (!opt) opt = &opt_default;

    printf("%s {\n", (type->category == INTRO_STRUCT)? "struct" : "union");

    for (int m_index = 0; m_index < type->i_struct->count_members; m_index++) {
        const IntroMember * m = &type->i_struct->members[m_index];
        const void * m_data = data + m->offset;
        for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
        printf("%s: ", m->name);
        intro_print_type_name(m->type);
        printf(" = ");
        if (intro_is_scalar(m->type)) {
            intro_print_basic(m_data, m->type);
        } else {
            switch(m->type->category) {
            case INTRO_ARRAY: {
                int depth;
                const IntroType * parent = m->type->parent;
                const int MAX_EXPOSED_LENGTH = 64;
                if (intro_is_scalar(parent) && m->type->array_size <= MAX_EXPOSED_LENGTH) {
                    intro_print_basic_array(m_data, parent, m->type->array_size);
                } else {
                    printf("<concealed>");
                }
            }break;

            case INTRO_POINTER: { // TODO
                void * ptr = *(void **)m_data;
                if (ptr) {
                    int depth;
                    const IntroType * base = intro_base(m->type, &depth);
                    if (depth == 1 && intro_is_scalar(base)) {
                        int64_t length;
                        if (intro_attribute_length(data, type, m, &length)) {
                            intro_print_basic_array(ptr, base, length);
                        } else {
                            if (strcmp(base->name, "char") == 0) {
                                char * str = (char *)ptr;
                                const int max_string_length = 32;
                                if (strlen(str) <= max_string_length) {
                                    printf("\"%s\"", str);
                                } else {
                                    printf("\"%*.s...\"", max_string_length - 3, str);
                                }
                            } else {
                                intro_print_basic_array(ptr, base, 1);
                            }
                        }
                    } else {
                        printf("%p", ptr);
                    }
                } else {
                    printf("<null>");
                }
            }break;

            case INTRO_STRUCT: // FALLTHROUGH
            case INTRO_UNION: {
                IntroPrintOptions opt2 = *opt;
                opt2.indent++;
                intro_print_struct(m_data, m->type, &opt2);
            }break;

            case INTRO_ENUM: {
                intro_print_enum(*(int *)m_data, m->type);
            }break;

            default: {
                printf("<unknown>");
            }break;
            }
        }
        printf(";\n");
    }
    for (int t=0; t < opt->indent; t++) fputs(tab, stdout);
    printf("}");
}

IntroType *
intro_type_with_name_ctx(IntroContext * ctx, const char * name) {
    for (int i=0; i < ctx->count_types; i++) {
        IntroType * type = &ctx->types[i];
        if (type->name && strcmp(type->name, name) == 0) {
            return type;
        }
    }
    return NULL;
}
