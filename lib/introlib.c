#include "intro.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define STB_DS_IMPLEMENTATION
#include "ext/stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "ext/stb_sprintf.h"

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#endif
#define strputnull(a) arrput(a,0)
// index of last put or get
#define shtemp(t) stbds_temp((t)-1)
#define hmtemp(t) stbds_temp((t)-1)

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
intro_origin(const IntroType * type, int * o_depth) {
    int depth = 0;
    while (type->parent && type->category != INTRO_ARRAY && type->category != INTRO_POINTER) {
        type = type->parent;
        depth++;
    }
    if (o_depth) *o_depth = depth;
    return type;
}

const char *
intro_enum_name(const IntroType * type, int value) {
    if (type->i_enum->is_sequential) {
        return type->i_enum->members[value].name;
    } else {
        for (int i=0; i < type->i_enum->count_members; i++) {
            if (type->i_enum->members[i].value == value) {
                return type->i_enum->members[i].name;
            }
        }
    }
    return NULL;
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

static void
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
    if (m->type->category == INTRO_STRUCT) {
        intro_set_values_ctx(ctx, dest + m->offset, m->type, value_attribute);
    } else if (intro_attribute_flag(m, INTRO_ATTR_TYPE)) {
        memcpy(dest + m->offset, &struct_type, sizeof(void *));
    } else if (intro_attribute_int(m, value_attribute, &value_offset)) {
        void * value_ptr = ctx->values + value_offset;
        if (m->type->category == INTRO_POINTER) {
            size_t data_offset = *(size_t *)value_ptr;
            void * data = ctx->values + data_offset;
            memcpy(dest + m->offset, &data, sizeof(size_t));
        } else {
            memcpy(dest + m->offset, value_ptr, size);
            intro_offset_pointers(dest + m->offset, m->type, ctx->values);
        }
    // TODO: this seems inelegant
    } else if (m->type->category == INTRO_ARRAY && m->type->parent->category == INTRO_STRUCT) {
        int elem_size = intro_size(m->type->parent);
        for (int i=0; i < m->type->array_size; i++) {
            void * elem_address = dest + m->offset + i * elem_size;
            intro_set_values_ctx(ctx, elem_address, m->type->parent, value_attribute);
        }
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

static void
intro_print_scalar(const void * data, const IntroType * type) {
    if (intro_is_scalar(type)) {
        if (type->category <= INTRO_S64) {
            int64_t value = intro_int_value(data, type);
            printf("%li", (long int)value);
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

static void
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

static void
intro_print_basic_array(const void * data, const IntroType * type, int length) {
    int elem_size = intro_size(type);
    if (elem_size) {
        printf("{");
        for (int i=0; i < length; i++) {
            if (i > 0) printf(", ");
            intro_print_scalar(data + elem_size * i, type);
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

static void
intro_print_struct_ctx(IntroContext * ctx, const void * data, const IntroType * type, const IntroPrintOptions * opt) {
    static const char * tab = "    ";

    if (type->category != INTRO_STRUCT && type->category != INTRO_UNION) {
        return;
    }

    printf("%s {\n", (type->category == INTRO_STRUCT)? "struct" : "union");

    for (int m_index = 0; m_index < type->i_struct->count_members; m_index++) {
        const IntroMember * m = &type->i_struct->members[m_index];
        const void * m_data = data + m->offset;
        for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
        printf("%s: ", m->name);
        intro_print_type_name(m->type);
        printf(" = ");
        if (intro_is_scalar(m->type)) {
            intro_print_scalar(m_data, m->type);
        } else {
            switch(m->type->category) {
            case INTRO_ARRAY: {
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
                    const IntroType * base = m->type->parent;
                    if (!base->parent && intro_is_scalar(base)) {
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
                intro_print_struct_ctx(ctx, m_data, m->type, &opt2);
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

void
intro_print_ctx(IntroContext * ctx, const void * data, const IntroType * type, const IntroPrintOptions * opt) {
    static const IntroPrintOptions opt_default = {0};

    if (!opt) {
        opt = &opt_default;
    }

    if (intro_is_scalar(type)) {
        intro_print_scalar(data, type);
    } else if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        intro_print_struct_ctx(ctx, data, type, opt);
    } else if (type->category == INTRO_ENUM) {
        int value = *(int *)data;
        intro_print_enum(value, type);
    } else if (type->category == INTRO_ARRAY && intro_is_scalar(type->parent)) {
        intro_print_basic_array(data, type, type->array_size);
    } else if (type->category == INTRO_POINTER) {
        void * value = *(void **)data;
        printf("%p", value);
    } else {
        printf("<unknown>");
    }
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

// CITY IMPLEMENTATION

static const int implementation_version_major = 0;
static const int implementation_version_minor = 1;

typedef struct {
    char magic_number [4];
    uint16_t version_major;
    uint16_t version_minor;
    uint8_t  size_info;
    uint8_t  reserved_0 [3];
    uint32_t data_ptr;
    uint32_t count_types;
} CityHeader;

static long
fsize(FILE * file) {
    long location = ftell(file);
    fseek(file, 0, SEEK_END);
    long result = ftell(file);
    fseek(file, location, SEEK_SET);
    return result;
}

char *
intro_read_file(const char * filename, size_t * o_size) {
    FILE * file = fopen(filename, "rb");
    if (!file) return NULL;
    size_t file_size = fsize(file);
    char * buffer = malloc(file_size + 1);
    if (file_size > 0) {
        if (fread(buffer, file_size, 1, file) != 1) {
            fclose(file);
            free(buffer);
            return NULL;
        }
        fclose(file);
    }
    buffer[file_size] = '\0';
    if (o_size) *o_size = file_size;
    return buffer;
}

int
intro_dump_file(const char * filename, void * data, size_t data_size) {
    FILE * file = fopen(filename, "wb");
    if (!file) return -1;
    int res = fwrite(data, data_size, 1, file);
    fclose(file);
    return (res == 1)? 0 : -1;
}

void * // data handle
intro_load_city_file_ctx(IntroContext * ctx, void * dest, const IntroType * dest_type, const char * filename) {
    size_t size;
    void * data = intro_read_file(filename, &size);
    if (!data) return NULL;

    intro_load_city_ctx(ctx, dest, dest_type, data, size);
    return data;
}

bool
intro_create_city_file(const char * filename, void * src, const IntroType * src_type) {
    size_t size;
    void * data = intro_create_city(src, src_type, &size);
    if (!data) return false;
    
    int error = intro_dump_file(filename, data, size);
    free(data);
    if (error < 0) {
        return false;
    } else {
        return true;
    }
}

static void
city__error(const char * msg) {
    fprintf(stderr, "CITY error: %s\n", msg);
}

static void
put_uint(uint8_t ** array, uint32_t number, uint8_t bytes) {
    memcpy(arraddnptr(*array, bytes), &number, bytes);
}

typedef struct {
    const IntroType * key;
} CityTypeSet;

typedef struct {
    void * key;
    uint32_t value;
} U32ByPtr;

typedef struct {
    uint8_t type_size;
    uint8_t offset_size;
    CityTypeSet * type_set;
    uint8_t * info;
    uint8_t * data;

    U32ByPtr * data_offset_by_ptr;
} CityCreationContext;

static uint32_t
city__get_serialized_id(CityCreationContext * ctx, const IntroType * type) {
    int type_id = hmgeti(ctx->type_set, type);
    if (type_id >= 0) {
        return type_id;
    }

    if (intro_is_scalar(type)) {
        put_uint(&ctx->info, type->category, 1);
    } else {
        switch(type->category) {
        case INTRO_ARRAY: {
            uint32_t parent_type_id = city__get_serialized_id(ctx, type->parent);
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, parent_type_id, ctx->type_size);
            put_uint(&ctx->info, type->array_size, 4);
        }break;

        case INTRO_POINTER: {
            uint32_t parent_type_id = city__get_serialized_id(ctx, type->parent);
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, parent_type_id, ctx->type_size);
        }break;

        case INTRO_ENUM: {
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, type->i_enum->size, 1);
        }break;

        case INTRO_UNION:
        case INTRO_STRUCT: {
            const IntroStruct * s_struct = type->i_struct;
            uint32_t m_type_ids [s_struct->count_members];
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                m_type_ids[m_index] = city__get_serialized_id(ctx, m->type);
            }

            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, s_struct->count_members, 4);
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                put_uint(&ctx->info, m_type_ids[m_index], ctx->type_size);

                put_uint(&ctx->info, m->offset, ctx->offset_size);

                size_t m_name_len = strlen(m->name);
                uint32_t name_offset = arrlen(ctx->data);
                memcpy(arraddnptr(ctx->data, m_name_len + 1), m->name, m_name_len + 1);
                put_uint(&ctx->info, name_offset, ctx->offset_size);
            }
        }break;

        default: break;
        }
    }

    hmputs(ctx->type_set, (CityTypeSet){type});
    type_id = hmtemp(ctx->type_set);
    return type_id;
}

// TODO: support ptr to ptr
static void
city__serialize_pointer_data(CityCreationContext * ctx, const IntroType * s_type) {
    assert(s_type->category == INTRO_STRUCT);

    for (int m_index=0; m_index < s_type->i_struct->count_members; m_index++) {
        const IntroMember * member = &s_type->i_struct->members[m_index];

        if (member->type->category == INTRO_POINTER) {
            void ** o_ptr = (void **)(ctx->data + member->offset);
            void * ptr = *o_ptr;
            if (!ptr) continue;

            if (hmgeti(ctx->data_offset_by_ptr, ptr) >= 0) {
                *o_ptr = (void *)(size_t)hmget(ctx->data_offset_by_ptr, ptr);
                continue;
            }

            int64_t length;
            if (intro_attribute_length(ctx->data, s_type, member, &length)) {
            } else if (member->type->parent->name && strcmp(member->type->parent->name, "char") == 0) {
                length = strlen((char *)ptr) + 1;
            } else {
                length = 1;
            }
            size_t alloc_size = intro_size(member->type->parent) * length;
            uint32_t * ser_length = (uint32_t *)arraddnptr(ctx->data, 4);
            *ser_length = length;

            void * ser_data = arraddnptr(ctx->data, alloc_size);
            memcpy(ser_data, ptr, alloc_size);

            uint32_t offset = ser_data - (void *)ctx->data;

            hmput(ctx->data_offset_by_ptr, ptr, offset);

            o_ptr = (void **)(ctx->data + member->offset);
            *o_ptr = (void *)(size_t)offset;
        }
    }
}

void *
intro_create_city(const void * src, const IntroType * s_type, size_t *o_size) {
    assert(s_type->category == INTRO_STRUCT);
    const IntroStruct * s_struct = s_type->i_struct;

    CityHeader header = {0};
    memcpy(header.magic_number, "ICTY", 4);
    header.version_major = implementation_version_major;
    header.version_minor = implementation_version_minor;

    CityCreationContext ctx_ = {0}, *ctx = &ctx_;

    // TODO: base on actual data size
    ctx->type_size = 2;
    ctx->offset_size = 3;
    header.size_info = ((ctx->type_size-1) << 4) | (ctx->offset_size-1);

    void * src_cpy = arraddnptr(ctx->data, s_struct->size);
    memcpy(src_cpy, src, s_struct->size);

    uint32_t main_type_id = city__get_serialized_id(ctx, s_type);
    uint32_t count_types = hmlenu(ctx->type_set);
    assert(main_type_id == count_types - 1);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arrlen(ctx->info);

    city__serialize_pointer_data(ctx, s_type);

    size_t result_size = sizeof(header) + arrlen(ctx->info) + arrlen(ctx->data);
    uint8_t * result = malloc(result_size);
    uint8_t * p = result;
    memcpy(p, &header, sizeof(header));
    p += sizeof(header);

    memcpy(p, ctx->info, arrlen(ctx->info));
    p += arrlen(ctx->info);

    memcpy(p, ctx->data, arrlen(ctx->data));

    arrfree(ctx->info);
    arrfree(ctx->data);
    hmfree(ctx->type_set);

    *o_size = result_size;
    return (void *)result;
}

static uint32_t
next_uint(const uint8_t ** ptr, uint8_t size) {
    uint32_t result = 0;
    //memcpy(&result + 4 - size, *ptr, size); // BE to BE
    memcpy(&result, *ptr, size); // LE to LE
    *ptr += size;
    return result;
}

static int
city__safe_copy_struct(
    IntroContext * ctx,
    void * restrict dest,
    const IntroType * restrict d_type,
    void * restrict src,
    const IntroType * restrict s_type
) {
    const IntroStruct * d_struct = d_type->i_struct;
    const IntroStruct * s_struct = s_type->i_struct;
    for (int dm_i=0; dm_i < d_struct->count_members; dm_i++) {
        const IntroMember * dm = &d_struct->members[dm_i];

        if (intro_attribute_flag(dm, INTRO_ATTR_TYPE)) {
            *(const IntroType **)(dest + dm->offset) = d_type;
            continue;
        }

        const char ** aliases = NULL;
        arrput(aliases, dm->name);
        for (int attr_i=0; attr_i < dm->count_attributes; attr_i++) {
            if (dm->attributes[attr_i].type == INTRO_ATTR_ALIAS) {
                const char * alias = ctx->notes[dm->attributes[attr_i].v.i];
                arrput(aliases, alias);
            }
        }

        bool found_match = false;
        for (int j=0; j < s_struct->count_members; j++) {
            const IntroMember * sm = &s_struct->members[j];

            bool match = false;
            for (int ai=0; ai < arrlen(aliases); ai++) {
                if (strcmp(aliases[ai], sm->name) == 0) {
                    match = true;
                    break;
                }
            }
            if (match) {
                if (dm->type->category != sm->type->category) {
                    char from [128];
                    char to [128];
                    char msg [512];
                    intro_sprint_type_name(from, sm->type);
                    intro_sprint_type_name(to,   dm->type);
                    stbsp_sprintf(msg, "type mismatch. from: %s to: %s", from, to);
                    city__error(msg);
                    return -1;
                }

                if (intro_is_scalar(dm->type) || dm->type->category == INTRO_ENUM) {
                    // TODO: should we test against enum names if things get moved around?
                    memcpy(dest + dm->offset, src + sm->offset, intro_size(dm->type));
                } else if (dm->type->category == INTRO_POINTER) {
                    void * result_ptr = src + *(size_t *)(src + sm->offset);
                    memcpy(dest + dm->offset, &result_ptr, sizeof(void *));
                } else if (dm->type->category == INTRO_ARRAY) {
                    if (dm->type->parent->category != sm->type->parent->category) {
                        city__error("array type mismatch");
                        return -1;
                    }
                    size_t d_size = intro_size(dm->type);
                    size_t s_size = intro_size(sm->type);
                    size_t size = (d_size > s_size)? s_size : d_size;
                    memcpy(dest + dm->offset, src + sm->offset, size);
                } else if (dm->type->category == INTRO_STRUCT) {
                    int ret = city__safe_copy_struct(ctx,
                        dest + dm->offset, dm->type,
                        src + sm->offset, sm->type
                    );
                    if (ret < 0) return ret;
                } else {
                    return -1;
                }
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            intro_set_member_value_ctx(ctx, dest, d_type, dm_i, INTRO_ATTR_DEFAULT);
        }
        arrfree(aliases);
    }

    return 0;
}

int
intro_load_city_ctx(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, size_t data_size) {
    const CityHeader * header = data;
    
    if (
        data_size < sizeof(*header)
     || memcmp(header->magic_number, "ICTY", 4) != 0
    ) {
        city__error("invalid CTY file");
        return -1;
    }

    if (header->version_major != implementation_version_major) {
        city__error("unsupported CTY version.");
        return -1;
    }

    if (header->version_minor > implementation_version_minor) {
        city__error("warning: some features will be unsupported.");
    }

    const uint8_t type_size   = 1 + ((header->size_info >> 4) & 0x0f);
    const uint8_t offset_size = 1 + ((header->size_info) & 0x0f) ;

    struct {
        uint32_t key;
        IntroType * value;
    } * info_by_id = NULL;

    uint8_t * src = data + header->data_ptr;
    const uint8_t * b = data + sizeof(*header);

    for (int i=0; i < header->count_types; i++) {
        IntroType * type = malloc(sizeof(*type));
        memset(type, 0, sizeof(*type));

        type->category = next_uint(&b, 1);

        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: {
            uint32_t count_members = next_uint(&b, 4);

            if ((void *)b + count_members * (type_size + offset_size + offset_size) > data + data_size) {
                city__error("malformed");
                return -1;
            }

            IntroMember * members = NULL;
            for (int m=0; m < count_members; m++) {
                IntroMember member;
                member.type   = hmget(info_by_id, next_uint(&b, type_size));
                member.offset = (int32_t)next_uint(&b, offset_size);
                member.name   = (char *)(src + next_uint(&b, offset_size));
                arrput(members, member);
            }

            IntroStruct struct_;
            struct_.count_members = count_members;
            struct_.is_union = type->category == INTRO_UNION;

            IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
            memcpy(result, &struct_, sizeof(struct_));
            memcpy(result->members, members, sizeof(*members) * arrlen(members));
            arrfree(members);

            type->i_struct = result;
        }break;

        case INTRO_POINTER: {
            uint32_t parent_id = next_uint(&b, type_size);

            IntroType * parent = hmget(info_by_id, parent_id);
            type->parent = parent;
        }break;

        case INTRO_ARRAY: {
            uint32_t parent_id = next_uint(&b, type_size);
            uint32_t array_size = next_uint(&b, 4);

            IntroType * parent = hmget(info_by_id, parent_id);
            type->parent = parent;
            type->array_size = array_size;
        }break;

        case INTRO_ENUM: {
            uint32_t size = next_uint(&b, 1);

            IntroEnum * i_enum = calloc(sizeof(*i_enum), 1);
            memset(i_enum, 0, sizeof(*i_enum));
            i_enum->size = size;

            type->i_enum = i_enum;
        }break;

        default: break;
        }
        hmput(info_by_id, i, type);
    }

    const IntroType * s_type = info_by_id[hmlen(info_by_id) - 1].value;

    int copy_result = city__safe_copy_struct(ctx, dest, d_type, src, s_type);

    for (int i=0; i < hmlen(info_by_id); i++) {
        if (intro_is_complex(info_by_id[i].value)) {
            free(info_by_id[i].value->i_struct);
        }
        free(info_by_id[i].value);
    }
    hmfree(info_by_id);

    return copy_result;
}
