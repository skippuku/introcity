#define INTRO_INCLUDE_INSTR_CODE
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

const static int MAX_EXPOSED_LENGTH = 64;
static const char * tab = "    ";

typedef uint8_t u8;

struct IntroPool {
    void ** ptrs;
    uint32_t count;
    uint32_t capacity;
};

const char *
intro_enum_name(const IntroType * type, int value) {
    if ((type->flags & INTRO_IS_SEQUENTIAL)) {
        return type->values[value].name;
    } else {
        for (uint32_t i=0; i < type->count; i++) {
            if (type->values[i].value == value) {
                return type->values[i].name;
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
    for (uint32_t i=0; i < bitset_index; i++) {
        pop += INTRO_POPCOUNT(spec->bitset[i]);
    }
    pop += INTRO_POPCOUNT(spec->bitset[bitset_index] & pop_count_mask);
    uint32_t * value_offsets = (uint32_t *)(spec + 1);
    *out = value_offsets[pop];

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
        const IntroMember * length_m = &container_type->members[length_m_index];
        const void * length_data = (u8 *)container + length_m->offset;
        int64_t result = intro_int_value(length_data, length_m->type);
        *o_length = result;
        return true;
    } else {
        *o_length = 0;
        return false;
    }
}

bool
intro_attribute_expr_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id, const void * cont_data, int64_t * o_result) {
    ASSERT_ATTR_TYPE(INTRO_AT_EXPR);
    uint32_t code_offset;
    bool has = get_attribute_value_offset(ctx, attr_spec_location, attr_id, &code_offset);
    if (has) {
        uint8_t * code = &ctx->values[code_offset];
        union IntroRegisterData reg = intro_run_bytecode(code, cont_data);
        *o_result = reg.si;
        return true;
    } else {
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

union IntroRegisterData
intro_run_bytecode(uint8_t * code, const uint8_t * data) {
    union IntroRegisterData stack [1024];
    union IntroRegisterData r0, r1, r2;
    size_t stack_idx = 0;
    size_t code_idx = 0;

    memset(&r1, 0, sizeof(r1)); // silence dumb warning

    while (1) {
        uint8_t byte = code[code_idx++];
        uint8_t inst = byte & ~0xC0;
        uint8_t size = 1 << ((byte >> 6) & 0x03);

        if (inst > I_GREATER_POP) {
            r1 = r0;
            r0 = stack[--stack_idx];
        }

        switch ((InstrCode)inst) {
        case I_RETURN: return r0;

        case I_LD: {
            memcpy(&r0, data + r0.ui, size);
        }break;

        case I_IMM: {
            stack[stack_idx++] = r0;
            r0.ui = 0;
            memcpy(&r0, &code[code_idx], size);
            code_idx += size;
        }break;

        case I_ZERO: {
            stack[stack_idx++] = r0;
            r0.ui = 0;
        }break;

        case I_CND_LD_TOP: {
            r1 = stack[--stack_idx]; // alternate value
            r2 = stack[--stack_idx]; // condition
            if (r2.ui) r0 = r1;
        }break;

        case I_NEGATE_I:   r0.si = -r0.si; break;
        case I_NEGATE_F:   r0.df = -r0.df; break;
        case I_BIT_NOT:    r0.ui = ~r0.ui; break;
        case I_NOT_ZERO:   r0.ui = !!(r0.ui); break;
        case I_CVT_D_TO_I: r0.si = (int64_t)r0.df; break;
        case I_CVT_F_TO_I: r0.si = (int64_t)r0.sf; break;
        case I_CVT_I_TO_D: r0.df = (double) r0.si; break;
        case I_CVT_F_TO_D: r0.df = (double) r0.sf; break;

        case I_ADDI: r0.si += r1.si; break;
        case I_MULI: r0.si *= r1.si; break;
        case I_DIVI: r0.si /= r1.si; break;
        case I_MODI: r0.si %= r1.si; break;

        case I_L_SHIFT: r0.ui <<= r1.ui; break;
        case I_R_SHIFT: r0.ui >>= r1.ui; break;

        case I_BIT_AND: r0.ui &= r1.ui; break;
        case I_BIT_OR:  r0.ui |= r1.ui; break;
        case I_BIT_XOR: r0.ui ^= r1.ui; break;

        case I_CMP: r0.ui = ((r0.si < r1.si) << 1) | (r0.si == r1.si); break;

        case I_ADDF: r0.df += r1.df; break;
        case I_MULF: r0.df *= r1.df; break;
        case I_DIVF: r0.df /= r1.df; break;

        case I_COUNT: case I_INVALID: assert(0);
        }
    }
}

static void
intro_offset_pointers(void * dest, const IntroType * type, void * base) {
    if (type->category == INTRO_ARRAY) {
        if (type->of->category == INTRO_POINTER) {
            for (uint32_t i=0; i < type->count; i++) {
                u8 ** o_ptr = (u8 **)((u8 *)dest + i * sizeof(void *));
                *o_ptr += (uintptr_t)base;
            }
        }
    }
}

void
intro_set_member_value_x(IntroContext * ctx, void * dest, const IntroType * struct_type, uint32_t member_index, uint32_t value_attribute) {
    const IntroMember * m = &struct_type->members[member_index];
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
        for (uint32_t i=0; i < m->type->count; i++) {
            void * elem_address = (u8 *)dest + m->offset + i * elem_size;
            intro_set_values_x(ctx, elem_address, m->type->of, value_attribute);
        }
    } else {
        memset((u8 *)dest + m->offset, 0, size);
    }
}

void
intro_set_values_x(IntroContext * ctx, void * dest, const IntroType * type, uint32_t value_attribute) {
    for (uint32_t m_index=0; m_index < type->count; m_index++) {
        intro_set_member_value_x(ctx, dest, type, m_index, value_attribute);
    }
}

void
intro_set_defaults_x(IntroContext * ctx, void * dest, const IntroType * type) {
    intro_set_values_x(ctx, dest, type, ctx->attr.builtin.i_default);
}

void
intro_sprint_type_name(char * dest, const IntroType * type) {
    while (1) {
        if (type->category == INTRO_POINTER) {
            *dest++ = '*';
            type = type->of;
        } else if (type->category == INTRO_ARRAY) {
            dest += 1 + stbsp_sprintf(dest, "[%u]", type->count) - 1;
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
intro__print_array(IntroContext * ctx, const IntroContainer * p_container, size_t length, const IntroPrintOptions * opt) {
    const IntroType * type = p_container->type->of;
    if (length <= MAX_EXPOSED_LENGTH) {
        if (intro_is_scalar(type)) {
            printf("{");
            for (uint32_t i=0; i < length; i++) {
                if (i > 0) printf(", ");
                intro_print_x(ctx, intro_push(p_container, i), opt);
            }
            printf("}");
        } else {
            printf("{\n");
            for (uint32_t i=0; i < length; i++) {
                for (int t=0; t < opt->indent + 2; t++) fputs(tab, stdout);
                IntroPrintOptions opt2 = *opt;
                opt2.indent += 2;
                intro_print_x(ctx, intro_push(p_container, i), &opt2);
                printf(",\n");
            }
            for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
            printf("}");
        }
    } else {
        printf("<concealed>");
    }
}

void
intro_print_x(IntroContext * ctx, IntroContainer container, const IntroPrintOptions * opt) {
    static const IntroPrintOptions opt_default = {0};
    const IntroType * type = container.type;
    const void * data = container.data;

    if (!opt) {
        opt = &opt_default;
    }

    uint32_t attr;
    if (container.parent && container.parent->type->category == INTRO_STRUCT) {
        attr = container.parent->type->members[container.index].attr;
    } else {
        attr = type->attr;
    }
    const char * fmt = intro_attribute_string_x(ctx, attr, ctx->attr.builtin.gui_format);

    switch(type->category) {
    case INTRO_U8: case INTRO_U16: case INTRO_U32: case INTRO_U64:
    case INTRO_S8: case INTRO_S16: case INTRO_S32: case INTRO_S64:
    {
        int64_t value = intro_int_value(data, type);
        printf((fmt)? fmt : "%li", (long int)value);
    }break;

    case INTRO_F32: {
        printf((fmt)? fmt : "%f", *(float *)data);
    }break;
    case INTRO_F64: {
        printf((fmt)? fmt : "%f", *(double *)data);
    }break;

    case INTRO_STRUCT:
    case INTRO_UNION: {
        printf("%s {\n", (type->category == INTRO_STRUCT)? "struct" : "union");

        for (uint32_t m_index = 0; m_index < type->count; m_index++) {
            const IntroMember * m = &type->members[m_index];
            const void * m_data = (u8 *)data + m->offset;
            for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
            printf("%s: ", m->name);
            intro_print_type_name(m->type);
            printf(" = ");
            IntroContainer next_container = intro_push(&container, m_index);
            if (intro_is_scalar(m->type)) {
                intro_print_x(ctx, next_container, opt);
            } else {
                switch(m->type->category) {
                case INTRO_ARRAY: {
                    intro_print_x(ctx, next_container, opt);
                }break;

                case INTRO_POINTER: {
                    void * ptr = *(void **)m_data;
                    if ((m_index > 0 && m->offset == type->members[m_index - 1].offset) || m->type->of->category == INTRO_UNKNOWN) {
                        printf("0x%016x", (int)(uintptr_t)ptr);
                        break;
                    }
                    if (ptr) {
                        intro_print_x(ctx, next_container, opt);
                    } else {
                        printf("<null>");
                    }
                }break;

                case INTRO_STRUCT: // FALLTHROUGH
                case INTRO_UNION: {
                    IntroPrintOptions opt2 = *opt;
                    opt2.indent++;
                    intro_print_x(ctx, next_container, &opt2);
                }break;

                case INTRO_ENUM: {
                    intro_print_x(ctx, next_container, opt);
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
    }break;

    case INTRO_ENUM: {
        int value = *(int *)data;
        if ((type->flags & INTRO_IS_SEQUENTIAL)) {
            if (value >= 0 && value < (int)type->count) {
                printf("%s", type->values[value].name);
            } else {
                printf("%i", value);
            }
        } else if ((type->flags & INTRO_IS_FLAGS)) {
            bool more_than_one = false;
            if (value) {
                for (uint32_t f=0; f < type->count; f++) {
                    if (value & type->values[f].value) {
                        if (more_than_one) printf(" | ");
                        printf("%s", type->values[f].name);
                        more_than_one = true;
                    }
                }
            } else {
                printf("0");
            }
        } else {
            printf("%i", value);
        }
    }break;

    case INTRO_ARRAY: {
        int64_t length = type->count;
        if (container.parent->type->category == INTRO_STRUCT) {
            if (!intro_attribute_length_x(ctx, container.parent->data, container.parent->type, &container.parent->type->members[container.index], &length)) {
                length = type->count;
            }
        }
        intro__print_array(ctx, &container, length, opt);
    }break;

    case INTRO_POINTER: {
        void * ptr = *(void **)data;
        if (!ptr) {
            printf("<null>");
        } else if (intro_has_attribute_x(ctx, attr, ctx->attr.builtin.i_cstring)) {
            char * str = (char *)ptr;
            const int max_string_length = 32;
            if (strlen(str) <= max_string_length) {
                printf("\"%s\"", str);
            } else {
                printf("\"%.*s...\"", max_string_length - 3, str);
            }
        } else {
            int64_t length;
            if (!intro_attribute_length_x(ctx, container.parent->data, container.parent->type, &container.parent->type->members[container.index], &length)) {
                length = 1;
            }
            intro__print_array(ctx, &container, length, opt);
        }
    }break;

    default:
        printf("<unknown>");
    }
}

IntroType *
intro_type_with_name_x(IntroContext * ctx, const char * name) {
    for (uint32_t i=0; i < ctx->count_types; i++) {
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
    for (uint32_t i=0; i < type->count; i++) {
        const IntroMember * member = &type->members[i];
        if (0==strcmp(name, member->name)) {
            return member;
        }
    }
    return NULL;
}

IntroContainer
intro_push(const IntroContainer * parent, int32_t index) {
    IntroContainer result;
    switch(parent->type->category) {
    case INTRO_POINTER: {
        result.type = parent->type->of;
        result.data = *(uint8_t **)(parent->data) + result.type->size * index;
    }break;

    case INTRO_ARRAY: {
        result.type = parent->type->of;
        result.data = parent->data + result.type->size * index;
    }break;

    case INTRO_STRUCT:
    case INTRO_UNION: {
        result.type = parent->type->members[index].type;
        result.data = parent->data + parent->type->members[index].offset;
    }break;

    default:
        result = *parent;
        return result;
    }

    result.index = index;
    result.parent = parent;
    return result;
}

// CITY IMPLEMENTATION

static const int implementation_version_major = 0;
static const int implementation_version_minor = 4;

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

bool // is_ok
intro_load_city_file_x(IntroContext * ctx, void * dest, const IntroType * dest_type, const char * filename) {
    size_t size;
    void * data = intro_read_file(filename, &size);
    if (!data) return false;

    intro_load_city_x(ctx, dest, dest_type, data, size);
    free(data);
    return true;
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
    uint32_t ser_offset;
    uint32_t size;
} CityBuffer;

typedef struct {
    uint32_t data_location;
    uint32_t ptr_value;
} CityDeferredPointer;

typedef struct {
    uint8_t * data;
    uint8_t * info;
    IntroContext * ictx;
    uint8_t type_size;
    uint8_t ptr_size;

    // Creation only
    uint32_t type_id_counter;
    CityTypeSet * type_set;
    CityBuffer * buffers;
    CityDeferredPointer * deferred_ptrs;
    struct{ char * key; uint32_t value; } * name_cache;
} CityContext;

#define CITY_INVALID_CACHE UINT32_MAX

static size_t
packed_size(const CityContext * city, const IntroType * type) {
    switch(type->category) {
    case INTRO_STRUCT: {
        size_t size = 0;
        for (uint32_t i=0; i < type->count; i++) {
            size += packed_size(city, type->members[i].type);
        }
        return size;
    }

    case INTRO_ARRAY:
        return type->count * type->of->size;

    case INTRO_POINTER:
        return city->ptr_size;

    default:
        return type->size;
    }
}

static inline uint64_t
alignoff(uint64_t len, uint64_t alignment) {
    uint64_t mask = alignment - 1;
    uint64_t off = (alignment - (len & mask)) & mask;
    return off;
}

static uint32_t
city__get_serialized_id(CityContext * ctx, const IntroType * type) {
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
            put_uint(&ctx->info, type->count, ctx->ptr_size);
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
            uint32_t * m_type_ids = (uint32_t *)malloc(type->count * sizeof(uint32_t));
            for (uint32_t m_index=0; m_index < type->count; m_index++) {
                const IntroMember * m = &type->members[m_index];
                m_type_ids[m_index] = city__get_serialized_id(ctx, m->type);
            }

            size_t id_test_bit = 1 << (ctx->ptr_size * 8 - 1);

            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, type->count, ctx->ptr_size);
            for (uint32_t m_index=0; m_index < type->count; m_index++) {
                const IntroMember * m = &type->members[m_index];
                put_uint(&ctx->info, m_type_ids[m_index], ctx->type_size);

                int32_t id;
                if (intro_attribute_int_x(ctx->ictx, m->attr, ctx->ictx->attr.builtin.i_id, &id)) {
                    size_t stored = id;
                    stored |= id_test_bit;
                    put_uint(&ctx->info, stored, ctx->ptr_size);
                } else {
                    uint32_t name_offset = shget(ctx->name_cache, m->name);
                    if (name_offset == CITY_INVALID_CACHE) {
                        size_t m_name_len = strlen(m->name);
                        name_offset = arrlen(ctx->data);
                        memcpy(arraddnptr(ctx->data, m_name_len + 1), m->name, m_name_len + 1);
                        shput(ctx->name_cache, m->name, name_offset);
                    }
                    put_uint(&ctx->info, name_offset, ctx->ptr_size);
                }
            }

            free(m_type_ids);
        }break;

        default: break;
        }
    }

    uint32_t type_id = ctx->type_id_counter++;
    hmput(ctx->type_set, type, type_id);
    return type_id;
}

static void
city__serialize(CityContext * ctx, uint32_t data_offset, const IntroType * type, const u8 * src, uint32_t elem_count) {
    switch(type->category) {
    case INTRO_STRUCT: {
        uint32_t current_offset = 0xFFffFFff;
        size_t next_offset = 0;
        for (uint32_t m_index=0; m_index < type->count; m_index++) {
            const IntroMember * member = &type->members[m_index];

            if (current_offset == next_offset) {
                continue;
            }
            current_offset = next_offset;
            next_offset += packed_size(ctx, member->type);

            if (!intro_has_attribute_x(ctx->ictx, member->attr, ctx->ictx->attr.builtin.i_city)) {
                memset(ctx->data + data_offset + current_offset, 0, packed_size(ctx, member->type));
                continue;
            }

            int64_t length;
            if (intro_attribute_length_x(ctx->ictx, src, type, member, &length)) {
            } else if (intro_has_attribute_x(ctx->ictx, member->attr, ctx->ictx->attr.builtin.i_cstring)) {
                char * str = *(char **)(src + member->offset);
                if (str) {
                    length = strlen(str) + 1;
                } else {
                    length = 0;
                }
            } else {
                length = 1;
            }

            city__serialize(ctx, data_offset + current_offset, member->type, src + member->offset, (uint32_t)length);
        }
    }break;

    case INTRO_POINTER: {
        if (type->of->size == 0) return;
        u8 * ptr = *(u8 **)src;
        if (!ptr) {
            memset(ctx->data + data_offset, 0, ctx->ptr_size);
            return;
        }

        uint32_t elem_size = packed_size(ctx, type->of);
        uint32_t buf_size = elem_size * elem_count;
        CityBuffer buf;
        CityDeferredPointer dptr;
        dptr.data_location = data_offset;
        for (int buf_i=0; buf_i < arrlen(ctx->buffers); buf_i++) {
            buf = ctx->buffers[buf_i];
            if (ptr == buf.origin && buf_size == buf.size) {
                dptr.ptr_value = buf.ser_offset;
                return;
            }
        }

        uint32_t * ser_length = (uint32_t *)arraddnptr(ctx->data, 4);
        memcpy(ser_length, &elem_count, 4);

        uint32_t ser_offset = arraddnindex(ctx->data, buf_size);

        buf.origin = ptr;
        buf.ser_offset = ser_offset;
        buf.size = buf_size;
        arrput(ctx->buffers, buf);

        dptr.ptr_value = ser_offset;
        arrput(ctx->deferred_ptrs, dptr);

        if (intro_is_scalar(type->of)) {
            memcpy(ctx->data + ser_offset, ptr, buf_size);
        } else {
            for (uint32_t elem_i=0; elem_i < elem_count; elem_i++) {
                uint32_t elem_offset = ser_offset + (elem_i * elem_size);
                city__serialize(ctx, elem_offset, type->of, ptr + (elem_i * type->of->size), 1);
            }
        }
    }break;

    case INTRO_ARRAY: {
        if (intro_is_scalar(type->of)) {
            memcpy(ctx->data + data_offset, src, type->size);
        } else {
            size_t ser_size = packed_size(ctx, type->of);
            for (uint32_t elem_i=0; elem_i < type->count; elem_i++) {
                uint32_t elem_offset = data_offset + (elem_i * ser_size);
                city__serialize(ctx, elem_offset, type->of, src + (elem_i * type->of->size), 1);
            }
        }
    }break;

    default: {
        memcpy(ctx->data + data_offset, src, type->size);
    }break;
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

    CityContext ctx_ = {0}, *ctx = &ctx_;

    shdefault(ctx->name_cache, CITY_INVALID_CACHE);
    ctx->ictx = ictx;

    // TODO: base on actual data size
    ctx->type_size = 2;
    ctx->ptr_size = 3;
    header.size_info = ((ctx->type_size-1) << 4) | (ctx->ptr_size-1);

    arraddnptr(ctx->data, packed_size(ctx, s_type));

    uint32_t main_type_id = city__get_serialized_id(ctx, s_type);
    uint32_t count_types = hmlenu(ctx->type_set);
    assert(main_type_id == count_types - 1);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arrlen(ctx->info);

    CityBuffer src_buf;
    src_buf.origin = (const u8 *)src;
    src_buf.ser_offset = 0;
    src_buf.size = packed_size(ctx, s_type);

    arrput(ctx->buffers, src_buf);
    city__serialize(ctx, 0, s_type, (u8 *)src, 1);

    for (int i=0; i < arrlen(ctx->deferred_ptrs); i++) {
        CityDeferredPointer dptr = ctx->deferred_ptrs[i];
        u8 * o_ptr = ctx->data + dptr.data_location;
        memcpy(o_ptr, &dptr.ptr_value, ctx->ptr_size);
    }
    arrfree(ctx->deferred_ptrs);

    size_t result_size = header.data_ptr + arrlen(ctx->data);
    u8 * result = (u8 *)malloc(result_size);
    u8 * p = result;
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
city__load_into(
    CityContext * city,
    void * restrict dest,
    const IntroType * restrict d_type,
    void * restrict src,
    const IntroType * restrict s_type,
    uint32_t count_elements
) {
    IntroContext * ctx = city->ictx;
    switch(d_type->category) {
    case INTRO_STRUCT:
    case INTRO_UNION: {
        const char ** aliases = NULL;
        uint32_t * skip_members = NULL;
        for (uint32_t dm_i=0; dm_i < d_type->count; dm_i++) {
            bool do_skip = false;
            for (int skip_i=0; skip_i < arrlen(skip_members); skip_i++) {
                if (skip_members[skip_i] == dm_i) {
                    if (arrlen(skip_members) > 1) {
                        arrdelswap(skip_members, dm_i);
                    } else {
                        arrsetlen(skip_members, 0);
                    }
                    do_skip = true;
                    break;
                }
            }
            if (do_skip) continue;

            const IntroMember * dm = &d_type->members[dm_i];

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
            for (uint32_t j=0; j < s_type->count; j++) {
                const IntroMember * sm = &s_type->members[j];

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

                    uint32_t length = 1;
                    if (dm->type->category == INTRO_POINTER) {
                        int32_t length_member_index;
                        const u8 * ptr_loc = (u8 *)src + sm->offset;
                        uintptr_t offset = next_uint(&ptr_loc, city->ptr_size);
                        if (offset == 0) break;
                        memcpy(&length, city->data + offset - 4, 4);
                        if (intro_attribute_member_x(ctx, dm->attr, ctx->attr.builtin.i_length, &length_member_index)) {
                            const IntroMember * lm = &d_type->members[length_member_index];
                            size_t wr_size = lm->type->size;
                            if (wr_size > 4) wr_size = 4;
                            memcpy((u8 *)dest + lm->offset, &length, wr_size);
                            if ((uint32_t)length_member_index > dm_i) {
                                arrput(skip_members, length_member_index);
                            }
                        }
                    }

                    int ret = city__load_into(city,
                        (u8 *)dest + dm->offset, dm->type,
                        (u8 *)src + sm->offset, sm->type,
                        length
                    );
                    if (ret < 0) return ret;
                    found_match = true;
                    break;
                }
            }
            if (!found_match) {
                intro_set_member_value_x(ctx, dest, d_type, dm_i, ctx->attr.builtin.i_default);
            }
            arrsetlen(aliases, 0);
        }
        arrfree(aliases);
        arrfree(skip_members);
    }break;

    case INTRO_POINTER: {
        const u8 * b = (u8 *)src;
        uintptr_t offset = next_uint(&b, city->ptr_size);
        if (offset != 0) {
            u8 * src_ptr = city->data + offset;

            u8 * dest_ptr = (u8 *)malloc(d_type->of->size * count_elements); // TODO: track
            if (intro_is_scalar(d_type->of)) {
                memcpy(dest_ptr, src_ptr, count_elements * d_type->of->size);
            } else {
                for (uint32_t i=0; i < count_elements; i++) {
                    size_t s_size = packed_size(city, s_type->of);
                    city__load_into(city, dest_ptr + (i * d_type->of->size), d_type->of, src_ptr + (i * s_size), s_type->of, 1);
                }
            }

            memcpy(dest, &dest_ptr, sizeof(void *));
        } else {
            memset(dest, 0, sizeof(void *));
        }
    }break;

    default: {
        memcpy(dest, src, d_type->size);
    }break;
    }

    return 0;
}

int
intro_load_city_x(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, size_t data_size) {
    CityContext _city = {0}, * city = &_city;
    const CityHeader * header = (const CityHeader *)data;
    city->ictx = ctx;
    
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

    city->type_size   = 1 + ((header->size_info >> 4) & 0x0f);
    city->ptr_size = 1 + ((header->size_info) & 0x0f) ;

    struct {
        uint32_t key;
        IntroType * value;
    } * info_by_id = NULL;

    city->data = (uint8_t *)data + header->data_ptr;
    const uint8_t * b = (u8 *)data + sizeof(*header);

    size_t id_test_bit = 1 << (city->ptr_size * 8 - 1);

    typedef struct {
        IntroType * type;
        uint32_t of_id;
    } TypePtrOf;
    TypePtrOf * deferred_pointer_ofs = NULL;

    for (uint32_t i=0; i < header->count_types; i++) {
        IntroType * type = (IntroType *)malloc(sizeof(*type)); // TODO: use arena allocator
        memset(type, 0, sizeof(*type));

        type->category = next_uint(&b, 1);

        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: {
            type->count = next_uint(&b, city->ptr_size);

            if (b + type->count * (city->type_size + city->ptr_size + city->ptr_size) > (u8 *)data + data_size) {
                city__error("malformed");
                return -1;
            }

            IntroMember * members = NULL;
            int32_t current_offset = 0;
            for (uint32_t m=0; m < type->count; m++) {
                IntroMember member = {0};
                uint32_t type_id = next_uint(&b, city->type_size);
                member.type   = hmget(info_by_id, type_id);
                member.offset = current_offset;
                current_offset += member.type->size;

                size_t next = next_uint(&b, city->ptr_size);
                if ((next & id_test_bit)) {
                    member.attr = next & (~id_test_bit); // store id directly in attr since that isn't being used for anything else
                } else {
                    member.name = (char *)(city->data + next);
                }

                arrput(members, member);
            }
            type->members = members;
            type->size = current_offset;
        }break;

        case INTRO_POINTER: {
            uint32_t of_id = next_uint(&b, city->type_size);

            TypePtrOf ptrof;
            ptrof.type = type;
            ptrof.of_id = of_id;

            arrput(deferred_pointer_ofs, ptrof);

            type->size = city->ptr_size;
        }break;

        case INTRO_ARRAY: {
            uint32_t elem_id = next_uint(&b, city->type_size);
            uint32_t count = next_uint(&b, city->ptr_size);

            IntroType * elem_type = hmget(info_by_id, elem_id);
            type->of = elem_type;
            type->count = count;
            type->size = elem_type->size * count;
        }break;

        case INTRO_ENUM: {
            uint32_t size = next_uint(&b, 1);

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

    int copy_result = city__load_into(city, dest, d_type, city->data, s_type, 1);

    for (int i=0; i < hmlen(info_by_id); i++) {
        IntroType * type = info_by_id[i].value;
        if (intro_has_members(type)) {
            arrfree(type->members);
        }
        free(type);
    }
    hmfree(info_by_id);

    return copy_result;
}
