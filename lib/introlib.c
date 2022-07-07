#include "intro.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#if defined(__GNUC__)
  #define INTRO_POPCOUNT __builtin_popcount
#elif defined(_MSC_VER)
  #include <intrin.h>
  #define INTRO_POPCOUNT __popcnt
#else
  #define INTRO_POPCOUNT intro_popcount_x
  // taken from https://github.com/BartMassey/popcount/blob/master/popcount.c
  INTRO_API_INLINE int
  intro_popcount_x(uint32_t x) {
      static const uint32_t m1 = 0x55555555;
      static const uint32_t m2 = 0x33333333;
      static const uint32_t m4 = 0x0f0f0f0f;
      x -= (x >> 1) & m1;
      x = (x & m2) + ((x >> 2) & m2);
      x = (x + (x >> 4)) & m4;
      x += x >>  8;
      return (x + (x >> 16)) & 0x3f;
  }
#endif

#if 0
static inline uint64_t
intro_bsr(uint64_t x) {
#if defined(__GNUC__)
    return 63 - __builtin_clzll(x);
#elif defined(_MSC_VER)
    uint64_t index;
    _BitScanReverse64(&index, x);
    return index;
#else
    uint64_t index = 63;
    for (; index > 0; index--) {
        if ((x & (1 << index))) {
            return index;
        }
    }
    return 0;
#endif
}
#endif

#ifdef __cplusplus
  #define restrict
#endif

typedef uint8_t u8;

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

static bool
get_attribute_value_offset(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id, uint32_t * out) {
    assert(attr_id < INTRO_MAX_ATTRIBUTES);
    IntroAttributeSpec * spec = &ctx->attr.spec_buffer[attr_spec_location];
    uint32_t bitset_index = attr_id >> 5; 
    uint32_t bit_index = attr_id & 31;
    uint32_t attr_bit = 1 << bit_index;
    uint32_t pop_count_mask = attr_bit - 1;
    bool has = spec->bitset[bitset_index] & attr_bit;
    if (!has) {
        return false;
    }

    int pop = 0;
    for (int i=0; i < bitset_index; i++) {
        pop += INTRO_POPCOUNT(spec->bitset[i]);
    }
    pop += INTRO_POPCOUNT(spec->bitset[bitset_index] & pop_count_mask);
    *out = spec->value_offsets[pop];

    return true;
}

#define ASSERT_ATTR_TYPE(a_type) assert(ctx->attr.available[attr_id].attr_type == a_type)

bool
intro_attribute_value_x(IntroContext * ctx, const IntroType * type, uint32_t attr_spec_location, uint32_t attr_id, IntroVariant * o_var) {
    ASSERT_ATTR_TYPE(INTRO_AT_VALUE);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &value_offset);
    if (!has) return false;

    o_var->data = ctx->values + value_offset;
    uint32_t type_id = ctx->attr.available[attr_id].type_id;
    o_var->type = (type_id != 0)? &ctx->types[type_id] : type;

    return true;
}

bool
intro_attribute_int_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id, int32_t * o_int) {
    ASSERT_ATTR_TYPE(INTRO_AT_INT);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &value_offset);
    if (has) {
        memcpy(o_int, &value_offset, sizeof(*o_int));
        return true;
    } else {
        return false;
    }
}

bool
intro_attribute_member_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id, int32_t * o_int) {
    ASSERT_ATTR_TYPE(INTRO_AT_MEMBER);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &value_offset);
    if (has) {
        memcpy(o_int, &value_offset, sizeof(*o_int));
        return true;
    } else {
        return false;
    }
}

bool
intro_attribute_float_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id, float * o_float) {
    ASSERT_ATTR_TYPE(INTRO_AT_FLOAT);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &value_offset);
    if (has) {
        memcpy(o_float, &value_offset, sizeof(*o_float));
        return true;
    } else {
        return false;
    }
}

const char *
intro_attribute_string_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id) {
    ASSERT_ATTR_TYPE(INTRO_AT_STRING);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &value_offset);
    if (has) {
        return ctx->strings[value_offset];
    } else {
        return NULL;
    }
}

bool
intro_attribute_length_x(IntroContext * ctx, const void * container, const IntroType * container_type, const IntroMember * m, int64_t * o_length) {
    assert(container_type->category == INTRO_STRUCT || container_type->category == INTRO_UNION);
    uint32_t value_offset;
    bool has = get_attribute_value_offset(ctx, m->attr, ctx->attr.builtin.i_length, &value_offset);
    if (has) {
        int32_t length_m_index;
        memcpy(&length_m_index, &value_offset, sizeof(length_m_index));
        const IntroMember * length_m = &container_type->i_struct->members[length_m_index];
        const void * length_data = (u8 *)container + length_m->offset;
        int64_t result = intro_int_value(length_data, length_m->type);
        *o_length = result;
        return true;
    } else {
        *o_length = 0;
        return false;
    }
}

#if 0
uint32_t
intro_attribute_id_by_string_literal_x(IntroContext * ctx, const char * str) {
    // NOTE: this hashes the pointer itself, not the string.
    // it is assumed that string literals will be used
    // but you should really avoid using this at all
    ptrdiff_t map_index = hmgeti(ctx->__attr_id_map, (char *)str);
    if (map_index >= 0) {
        return ctx->__attr_id_map[map_index].value;
    } else {
        uint32_t i = 0;
        while (i < ctx->attr.count_available) {
            if (0==strcmp(ctx->attr.available[i].name, (char *)str)) {
                break;
            }
            i++;
        }
        if (i >= ctx->attr.count_available) i = UINT32_MAX; // represents no match
        hmput(ctx->__attr_id_map, (char *)str, i);
        return i;
    }
}
#endif

static void
intro_offset_pointers(void * dest, const IntroType * type, void * base) {
    if (type->category == INTRO_ARRAY) {
        if (type->of->category == INTRO_POINTER) {
            for (int i=0; i < type->array_size; i++) {
                u8 ** o_ptr = (u8 **)((u8 *)dest + i * sizeof(void *));
                *o_ptr += (uintptr_t)base;
            }
        }
    }
}

void
intro_set_member_value_ctx(IntroContext * ctx, void * dest, const IntroType * struct_type, uint32_t member_index, uint32_t value_attribute) {
    const IntroMember * m = &struct_type->i_struct->members[member_index];
    size_t size = intro_size(m->type);
    IntroVariant var;
    if (intro_has_attribute_x(ctx, m->attr, ctx->attr.builtin.i_type)) {
        memcpy((u8 *)dest + m->offset, &struct_type, sizeof(void *));
    } else if (intro_attribute_value_x(ctx, m->type, m->attr, value_attribute, &var)) {
        assert(var.type == m->type);
        void * value_ptr = var.data;
        if (m->type->category == INTRO_POINTER) {
            size_t data_offset;
            memcpy(&data_offset, value_ptr, sizeof(size_t));
            void * data = ctx->values + data_offset;
            memcpy((u8 *)dest + m->offset, &data, sizeof(size_t));
        } else {
            memcpy((u8 *)dest + m->offset, value_ptr, size);
            intro_offset_pointers((u8 *)dest + m->offset, m->type, ctx->values);
        }
    } else if (m->type->category == INTRO_STRUCT) {
        intro_set_values_x(ctx, (u8 *)dest + m->offset, m->type, value_attribute);
    // TODO: this seems inelegant
    } else if (m->type->category == INTRO_ARRAY && m->type->of->category == INTRO_STRUCT) {
        int elem_size = intro_size(m->type->of);
        for (int i=0; i < m->type->array_size; i++) {
            void * elem_address = (u8 *)dest + m->offset + i * elem_size;
            intro_set_values_x(ctx, elem_address, m->type->of, value_attribute);
        }
    } else {
        memset((u8 *)dest + m->offset, 0, size);
    }
}

void
intro_set_values_x(IntroContext * ctx, void * dest, const IntroType * type, uint32_t value_attribute) {
    for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
        intro_set_member_value_ctx(ctx, dest, type, m_index, value_attribute);
    }
}

void
intro_set_defaults_ctx(IntroContext * ctx, void * dest, const IntroType * type) {
    intro_set_values_x(ctx, dest, type, ctx->attr.builtin.i_default);
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
            intro_print_scalar((u8 *)data + elem_size * i, type);
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
            type = type->of;
        } else if (type->category == INTRO_ARRAY) {
            dest += 1 + stbsp_sprintf(dest, "[%u]", type->array_size) - 1;
            type = type->of;
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

    printf("%s {\n", (type->category == INTRO_STRUCT)? "struct" : "union");

    for (int m_index = 0; m_index < type->i_struct->count_members; m_index++) {
        const IntroMember * m = &type->i_struct->members[m_index];
        const void * m_data = (u8 *)data + m->offset;
        for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
        printf("%s: ", m->name);
        intro_print_type_name(m->type);
        printf(" = ");
        if (intro_is_scalar(m->type)) {
            intro_print_scalar(m_data, m->type);
        } else {
            switch(m->type->category) {
            case INTRO_ARRAY: {
                const IntroType * of = m->type->of;
                const int MAX_EXPOSED_LENGTH = 64;
                if (intro_is_scalar(of) && m->type->array_size <= MAX_EXPOSED_LENGTH) {
                    intro_print_basic_array(m_data, of, m->type->array_size);
                } else {
                    printf("<concealed>");
                }
            }break;

            case INTRO_POINTER: { // TODO
                void * ptr = *(void **)m_data;
                if (ptr) {
                    const IntroType * base = m->type->of;
                    if (intro_is_scalar(base)) {
                        int64_t length;
                        if (intro_attribute_length_x(ctx, data, type, m, &length)) {
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
    } else if (type->category == INTRO_ARRAY && intro_is_scalar(type->of)) {
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

const IntroMember *
intro_member_by_name_x(const IntroType * type, const char * name) {
    assert((type->category & 0xf0) == INTRO_STRUCT);
    for (int i=0; i < type->i_struct->count_members; i++) {
        const IntroMember * member = &type->i_struct->members[i];
        if (0==strcmp(name, member->name)) {
            return member;
        }
    }
    return NULL;
}

// CITY IMPLEMENTATION

static const int implementation_version_major = 0;
static const int implementation_version_minor = 3;

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
    char * buffer = (char *)malloc(file_size + 1);
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
intro_create_city_file_x(IntroContext * ctx, const char * filename, void * src, const IntroType * src_type) {
    size_t size;
    void * data = intro_create_city_x(ctx, src, src_type, &size);
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
    uint32_t value;
} CityTypeSet;

typedef struct {
    const u8 * origin;
    int32_t ser_offset;
    uint32_t size;
} CityBuffer;

typedef struct {
    uint32_t data_location;
    uint32_t offset_value;
} CityDeferredPointer;

typedef struct {
    uint32_t type_id_counter;
    uint8_t type_size;
    uint8_t offset_size;
    uint8_t * info;
    uint8_t * data;

    CityTypeSet * type_set;
    IntroContext * ictx;
    CityBuffer * buffers;
    CityDeferredPointer * deferred_ptrs;
    struct{ char * key; uint32_t value; } * name_cache;
} CityCreationContext;

#define CITY_INVALID_CACHE UINT32_MAX

static inline uint64_t
alignoff(uint64_t len, uint64_t alignment) {
    uint64_t mask = alignment - 1;
    uint64_t off = (alignment - (len & mask)) & mask;
    return off;
}

static void
align_arr_to(uint8_t ** array, uint64_t alignment, uint8_t byte) {
    uint64_t pad = alignoff(arrlenu(*array), alignment);
    memset(arraddnptr(*array, pad), byte, pad);
}

static uint32_t
city__get_serialized_id(CityCreationContext * ctx, const IntroType * type) {
    ptrdiff_t type_id_index = hmgeti(ctx->type_set, type);
    if (type_id_index >= 0) {
        return ctx->type_set[type_id_index].value;
    }

    if (intro_is_scalar(type)) {
        put_uint(&ctx->info, type->category, 1);
    } else {
        switch(type->category) {
        case INTRO_ARRAY: {
            uint32_t elem_type_id = city__get_serialized_id(ctx, type->of);
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, elem_type_id, ctx->type_size);
            put_uint(&ctx->info, type->array_size, 4);
        }break;

        case INTRO_POINTER: {
            uint32_t ptr_type_id = ctx->type_id_counter++;
            hmput(ctx->type_set, type, ptr_type_id);

            put_uint(&ctx->info, type->category, 1);

            size_t of_type_id_index = arraddnindex(ctx->info, ctx->type_size);

            uint32_t of_type_id = city__get_serialized_id(ctx, type->of);
            memcpy(&ctx->info[of_type_id_index], &of_type_id, ctx->type_size);

            return ptr_type_id;
        }break;

        case INTRO_ENUM: {
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, type->size, 1);
        }break;

        case INTRO_UNION:
        case INTRO_STRUCT: {
            const IntroStruct * s_struct = type->i_struct;
            uint32_t m_type_ids [s_struct->count_members];
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                m_type_ids[m_index] = city__get_serialized_id(ctx, m->type);
            }

            size_t id_test_bit = 1 << (ctx->offset_size * 8 - 1);

            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, s_struct->count_members, 4);
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                put_uint(&ctx->info, m_type_ids[m_index], ctx->type_size);

                put_uint(&ctx->info, m->offset, ctx->offset_size);

                int32_t id;
                if (intro_attribute_int_x(ctx->ictx, m->attr, ctx->ictx->attr.builtin.i_id, &id)) {
                    size_t stored = id;
                    stored |= id_test_bit;
                    put_uint(&ctx->info, stored, ctx->offset_size);
                } else {
                    uint32_t name_offset = shget(ctx->name_cache, m->name);
                    if (name_offset == CITY_INVALID_CACHE) {
                        size_t m_name_len = strlen(m->name);
                        name_offset = arrlen(ctx->data);
                        memcpy(arraddnptr(ctx->data, m_name_len + 1), m->name, m_name_len + 1);
                        shput(ctx->name_cache, m->name, name_offset);
                    }
                    put_uint(&ctx->info, name_offset, ctx->offset_size);
                }
            }
        }break;

        default: break;
        }
    }

    uint32_t type_id = ctx->type_id_counter++;
    hmput(ctx->type_set, type, type_id);
    return type_id;
}

// TODO: support ptr to ptr, array of ptr
static void
city__serialize_pointer_data(CityCreationContext * ctx, uint32_t data_offset, const IntroType * type, uint32_t elem_count) {
    switch(type->category) {
    case INTRO_STRUCT: {
        uint32_t last_offset = 0xFFffFFff;
        for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
            const IntroMember * member = &type->i_struct->members[m_index];

            if (member->offset == last_offset) {
                continue;
            }
            last_offset = member->offset;

            if (!intro_has_attribute_x(ctx->ictx, member->attr, ctx->ictx->attr.builtin.i_city)) {
                memset(ctx->data + data_offset + member->offset, 0, member->type->size);
                continue;
            }

            int64_t length;
            if (intro_attribute_length_x(ctx->ictx, ctx->data + data_offset, type, member, &length)) {
            } else if (intro_has_attribute_x(ctx->ictx, member->attr, ctx->ictx->attr.builtin.i_cstring)) {
                char * str = *(char **)(ctx->data + data_offset + member->offset);
                if (str) {
                    length = strlen(str) + 1;
                } else {
                    length = 0;
                }
            } else {
                length = 1;
            }

            city__serialize_pointer_data(ctx, data_offset + member->offset, member->type, (uint32_t)length);
        }
    }break;

    case INTRO_POINTER: {
        if (type->of->size == 0) return;
        u8 * ptr = *(u8 **)(ctx->data + data_offset);
        if (!ptr) return;

        uint32_t elem_size = type->of->size;
        uint32_t buf_size = elem_size * elem_count;
        CityBuffer buf;
        CityDeferredPointer dptr;
        dptr.data_location = data_offset;
        for (int buf_i=0; buf_i < arrlen(ctx->buffers); buf_i++) {
            buf = ctx->buffers[buf_i];
            if (ptr >= buf.origin && ptr + buf_size <= buf.origin + buf.size) {
                dptr.offset_value = ptr - buf.origin + buf.ser_offset,
                arrput(ctx->deferred_ptrs, dptr);
                return;
            }
        }

        align_arr_to(&ctx->data, 4, 0x7e);
        uint32_t * ser_length = (uint32_t *)arraddnptr(ctx->data, 4);
        *ser_length = elem_count;

        uint32_t alignment = type->of->align;
        align_arr_to(&ctx->data, alignment, 0x2b);
        uint32_t ser_offset = arraddnindex(ctx->data, buf_size);
        memcpy(ctx->data + ser_offset, ptr, buf_size);

        buf.origin = ptr;
        buf.ser_offset = ser_offset;
        buf.size = buf_size;
        arrput(ctx->buffers, buf);

        dptr.offset_value = ser_offset,
        arrput(ctx->deferred_ptrs, dptr);

        for (int elem_i=0; elem_i < elem_count; elem_i++) {
            uint32_t elem_offset = ser_offset + (elem_i * type->of->size);
            city__serialize_pointer_data(ctx, elem_offset, type->of, 1);
        }
    }break;

    case INTRO_ARRAY: {
        for (int elem_i=0; elem_i < type->array_size; elem_i++) {
            uint32_t elem_offset = data_offset + (elem_i * type->of->size);
            city__serialize_pointer_data(ctx, elem_offset, type->of, 1);
        }
    }break;

    default: break;
    }
}

void *
intro_create_city_x(IntroContext * ictx, const void * src, const IntroType * s_type, size_t *o_size) {
    assert(s_type->category == INTRO_STRUCT);

    CityHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic_number, "ICTY", 4);
    header.version_major = implementation_version_major;
    header.version_minor = implementation_version_minor;

    CityCreationContext ctx_ = {0}, *ctx = &ctx_;

    shdefault(ctx->name_cache, CITY_INVALID_CACHE);
    ctx->ictx = ictx;
    // TODO: base on actual data size
    ctx->type_size = 2;
    ctx->offset_size = 3;
    header.size_info = ((ctx->type_size-1) << 4) | (ctx->offset_size-1);

    void * src_cpy = arraddnptr(ctx->data, s_type->size);
    memcpy(src_cpy, src, s_type->size);

    uint32_t main_type_id = city__get_serialized_id(ctx, s_type);

    uint32_t count_types = hmlenu(ctx->type_set);
    assert(main_type_id == count_types - 1);

    uint64_t pad = alignoff(sizeof(header) + arrlen(ctx->info), 8);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arrlen(ctx->info) + pad;

    CityBuffer src_buf;
    src_buf.origin = (const u8 *)src;
    src_buf.ser_offset = 0;
    src_buf.size = s_type->size;

    arrput(ctx->buffers, src_buf);
    city__serialize_pointer_data(ctx, 0, s_type, 1);

    for (int i=0; i < arrlen(ctx->deferred_ptrs); i++) {
        CityDeferredPointer dptr = ctx->deferred_ptrs[i];
        uintptr_t * o_ptr = (uintptr_t *)((u8 *)ctx->data + dptr.data_location);
        *o_ptr = dptr.offset_value;
    }
    arrfree(ctx->deferred_ptrs);

    size_t result_size = header.data_ptr + arrlen(ctx->data);
    uint8_t * result = (uint8_t *)malloc(result_size);
    uint8_t * p = result;
    memcpy(p, &header, sizeof(header));
    p += sizeof(header);

    memcpy(p, ctx->info, arrlen(ctx->info));
    p += arrlen(ctx->info);

    memset(p, 0x3D, pad);
    p += pad;

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
    const char ** aliases = NULL;
    uint32_t * skip_members = NULL;
    for (int dm_i=0; dm_i < d_struct->count_members; dm_i++) {
        bool do_skip = false;
        for (int skip_i=0; skip_i < arrlen(skip_members); skip_i++) {
            if (skip_members[skip_i] == dm_i) {
                arrdelswap(skip_members, dm_i);
                do_skip = true;
                break;
            }
        }
        if (do_skip) continue;

        const IntroMember * dm = &d_struct->members[dm_i];

        if (intro_has_attribute_x(ctx, dm->attr, ctx->attr.builtin.i_type)) {
            *(const IntroType **)((u8 *)dest + dm->offset) = d_type;
            continue;
        }

        arrput(aliases, dm->name);
        const char * alias;
        if ((alias = intro_attribute_string_x(ctx, dm->attr, ctx->attr.builtin.i_alias)) != NULL) {
            arrput(aliases, alias);
        }

        bool found_match = false;
        // TODO: it would probably be faster to build a hash lookup during the type parse so this isn't slow searching
        for (int j=0; j < s_struct->count_members; j++) {
            const IntroMember * sm = &s_struct->members[j];

            bool match = false;
            if (sm->name) {
                for (int alias_i=0; alias_i < arrlen(aliases); alias_i++) {
                    if (strcmp(aliases[alias_i], sm->name) == 0) {
                        match = true;
                        break;
                    }
                }
            } else {
                int32_t sm_id = sm->attr;
                int32_t dm_id;
                if (intro_attribute_int_x(ctx, dm->attr, ctx->attr.builtin.i_id, &dm_id) && dm_id == sm_id) {
                    match = true;
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
                    memcpy((u8 *)dest + dm->offset, (u8 *)src + sm->offset, intro_size(dm->type));
                } else if (dm->type->category == INTRO_POINTER) {
                    uintptr_t offset = *(uintptr_t *)((u8 *)src + sm->offset);
                    if (offset != 0) {
                        void * result_ptr = (u8 *)src + offset;
                        uint32_t * length_ptr = (uint32_t *)result_ptr - 1;
                        memcpy((u8 *)dest + dm->offset, &result_ptr, sizeof(void *));
                        int32_t length_member_index;
                        if (intro_attribute_member_x(ctx, dm->attr, ctx->attr.builtin.i_length, &length_member_index)) {
                            const IntroMember * lm = &d_type->i_struct->members[length_member_index];
                            size_t wr_size = intro_size(lm->type);
                            if (wr_size > 4) wr_size = 4;
                            memcpy((u8 *)dest + lm->offset, length_ptr, wr_size);
                            if (length_member_index > dm_i) {
                                arrput(skip_members, length_member_index);
                            }
                        }
                    } else {
                        memset((u8 *)dest + dm->offset, 0, sizeof(void *));
                    }
                } else if (dm->type->category == INTRO_ARRAY) {
                    if (dm->type->of->category != sm->type->of->category) {
                        city__error("array type mismatch");
                        return -1;
                    }
                    size_t d_size = intro_size(dm->type);
                    size_t s_size = intro_size(sm->type);
                    size_t size = (d_size > s_size)? s_size : d_size;
                    memcpy((u8 *)dest + dm->offset, (u8 *)src + sm->offset, size);
                } else if (dm->type->category == INTRO_STRUCT) {
                    int ret = city__safe_copy_struct(ctx,
                        (u8 *)dest + dm->offset, dm->type,
                        (u8 *)src + sm->offset, sm->type
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
            intro_set_member_value_ctx(ctx, dest, d_type, dm_i, ctx->attr.builtin.i_default);
        }
        arrsetlen(aliases, 0);
    }
    arrfree(aliases);
    arrfree(skip_members);

    return 0;
}

int
intro_load_city_ctx(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, size_t data_size) {
    const CityHeader * header = (const CityHeader *)data;
    
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

    uint8_t * src = (uint8_t *)data + header->data_ptr;
    const uint8_t * b = (uint8_t *)data + sizeof(*header);

    size_t id_test_bit = 1 << (offset_size * 8 - 1);

    typedef struct {
        IntroType * type;
        uint32_t of_id;
    } TypePtrOf;
    TypePtrOf * deferred_pointer_ofs = NULL;

    for (int i=0; i < header->count_types; i++) {
        IntroType * type = (IntroType *)malloc(sizeof(*type)); // TODO: use arena allocator
        memset(type, 0, sizeof(*type));

        type->category = next_uint(&b, 1);

        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: {
            uint32_t count_members = next_uint(&b, 4);

            if (b + count_members * (type_size + offset_size + offset_size) > (uint8_t *)data + data_size) {
                city__error("malformed");
                return -1;
            }

            IntroMember * members = NULL;
            for (int m=0; m < count_members; m++) {
                IntroMember member = {0};
                uint32_t type_id = next_uint(&b, type_size);
                member.type   = hmget(info_by_id, type_id);
                member.offset = (int32_t)next_uint(&b, offset_size);

                size_t next = next_uint(&b, offset_size);
                if ((next & id_test_bit)) {
                    member.attr = next & (~id_test_bit); // store id directly in attr since that isn't being used for anything else
                } else {
                    member.name = (char *)(src + next);
                }

                arrput(members, member);
            }

            IntroStruct struct_;
            struct_.count_members = count_members;
            struct_.is_union = type->category == INTRO_UNION;

            IntroStruct * result = (IntroStruct *)malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
            memcpy(result, &struct_, sizeof(struct_));
            memcpy(result->members, members, sizeof(*members) * arrlen(members));
            arrfree(members);

            type->i_struct = result;
        }break;

        case INTRO_POINTER: {
            uint32_t of_id = next_uint(&b, type_size);

            TypePtrOf ptrof;
            ptrof.type = type;
            ptrof.of_id = of_id;

            arrput(deferred_pointer_ofs, ptrof);

            type->size = 8;
        }break;

        case INTRO_ARRAY: {
            uint32_t elem_id = next_uint(&b, type_size);
            uint32_t array_size = next_uint(&b, 4);

            IntroType * elem_type = hmget(info_by_id, elem_id);
            type->of = elem_type;
            type->array_size = array_size;
            type->size = elem_type->size * array_size;
        }break;

        case INTRO_ENUM: {
            uint32_t size = next_uint(&b, 1);

            IntroEnum * i_enum = (IntroEnum *)calloc(sizeof(*i_enum), 1);
            memset(i_enum, 0, sizeof(*i_enum));

            type->i_enum = i_enum;
            type->size = size;
        }break;

        default: break;
        }
        if (type->size == 0) {
            switch(type->category) {
            case INTRO_U8: case INTRO_S8:                   type->size = 1; break;
            case INTRO_U16: case INTRO_S16:                 type->size = 2; break;
            case INTRO_U32: case INTRO_S32: case INTRO_F32: type->size = 4; break;
            case INTRO_U64: case INTRO_S64: case INTRO_F64: type->size = 8; break;
            case INTRO_F128: type->size = 16; break;
            }
        }
        hmput(info_by_id, i, type);
    }

    for (int i=0; i < arrlen(deferred_pointer_ofs); i++) {
        TypePtrOf ptrof = deferred_pointer_ofs[i];
        ptrof.type->of = hmget(info_by_id, ptrof.of_id);
    }
    arrfree(deferred_pointer_ofs);

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
