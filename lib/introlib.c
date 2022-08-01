#define INTRO_INCLUDE_EXTRA
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

void *
arena_alloc(MemArena * arena, size_t amount) {
    if (arena->current_used + amount > arena->capacity) {
        if (amount <= arena->capacity) {
            if (arena->buckets[++arena->current].data == NULL) {
                arena->buckets[arena->current].data = calloc(1, arena->capacity);
            }
            arena->current_used = 0;
        } else {
            arena->current++;
            arena->buckets[arena->current].data = realloc(arena->buckets[arena->current].data, amount);
            memset(arena->buckets[arena->current].data, 0, amount - arena->capacity);
        }
    }
    void * result = arena->buckets[arena->current].data + arena->current_used;
    arena->current_used += amount;
    arena->current_used += 16 - (arena->current_used & 15);
    return result;
}

MemArena *
new_arena(int capacity) {
    MemArena * arena = calloc(1, sizeof(MemArena));
    arena->capacity = capacity;
    arena->buckets[0].data = calloc(1, arena->capacity);
    return arena;
}

void
reset_arena(MemArena * arena) {
    for (int i=0; i <= arena->current; i++) {
        memset(arena->buckets[i].data, 0, arena->capacity);
    }
    arena->current = 0;
    arena->current_used = 0;
}

void
free_arena(MemArena * arena) {
    for (int i=0; i < LENGTH(arena->buckets); i++) {
        if (arena->buckets[i].data) free(arena->buckets[i].data);
    }
    free(arena);
}

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
    if (o_var->type->category == INTRO_POINTER) {
        uintptr_t val = *(uintptr_t *)o_var->data;
        o_var->data = ctx->values + val;
    }

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

bool
intro_attribute_length_x(IntroContext * ctx, IntroContainer cntr, int64_t * o_length) {
    assert(cntr.parent && intro_has_members(cntr.parent->type));
    return intro_attribute_expr_x(ctx, cntr, ctx->attr.builtin.i_length, o_length);
}

static const void *
intro_expr_data(const IntroContainer * pcntr) {
    if (pcntr->parent) {
        pcntr = pcntr->parent;
    }
    while (pcntr->parent && (pcntr->type->flags & INTRO_EMBEDDED_DEFINITION)) {
        pcntr = pcntr->parent;
    }
    return pcntr->data;
}

bool
intro_attribute_expr_x(IntroContext * ctx, IntroContainer cntr, uint32_t attr_id, int64_t * o_result) {
    ASSERT_ATTR_TYPE(INTRO_AT_EXPR);
    uint32_t code_offset;
    const void * data = intro_expr_data(&cntr);
    bool has = get_attribute_value_offset(ctx, intro_get_attr(cntr), attr_id, &code_offset);
    if (has) {
        uint8_t * code = &ctx->values[code_offset];
        union IntroRegisterData reg = intro_run_bytecode(code, data);
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
intro__offset_pointers(void * dest, const IntroType * type, void * base) {
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
    size_t size = m->type->size;
    IntroVariant var;
    if (intro_has_attribute_x(ctx, m->attr, ctx->attr.builtin.i_type)) {
        memcpy((u8 *)dest + m->offset, &struct_type, sizeof(void *));
    } else if (intro_attribute_value_x(ctx, m->type, m->attr, value_attribute, &var)) {
        assert(var.type == m->type);
        void * value_ptr = var.data;
        if (m->type->category == INTRO_POINTER) {
            memcpy((u8 *)dest + m->offset, &value_ptr, sizeof(size_t));
        } else {
            memcpy((u8 *)dest + m->offset, value_ptr, size);
            intro__offset_pointers((u8 *)dest + m->offset, m->type, ctx->values);
        }
    } else if (m->type->category == INTRO_STRUCT) {
        intro_set_values_x(ctx, (u8 *)dest + m->offset, m->type, value_attribute);
    // TODO: this seems inelegant
    } else if (m->type->category == INTRO_ARRAY && m->type->of->category == INTRO_STRUCT) {
        int elem_size = m->type->of->size;
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
            dest += stbsp_sprintf(dest, "[%u]", type->count);
            type = type->of;
        } else if (type->name) {
            dest += stbsp_sprintf(dest, "%s", type->name);
            break;
        } else {
            dest += stbsp_sprintf(dest, "<anon>");
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
    const char * fmt = NULL;
    IntroVariant var;
    if (intro_attribute_value_x(ctx, NULL, attr, ctx->attr.builtin.gui_format, &var)) {
        fmt = (char *)var.data;
    }

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
            IntroContainer m_cntr = intro_push(&container, m_index);

            int64_t expr_result;
            if (
                !intro_has_attribute_x(ctx, m->attr, ctx->attr.builtin.gui_show)
              ||(intro_attribute_expr_x(ctx, m_cntr, ctx->attr.builtin.i_when, &expr_result) && !expr_result)
               )
            {
                continue;
            }

            const void * m_data = (u8 *)data + m->offset;
            for (int t=0; t < opt->indent + 1; t++) fputs(tab, stdout);
            printf("%s: ", m->name);
            intro_print_type_name(m->type);
            printf(" = ");
            if (intro_is_scalar(m->type)) {
                intro_print_x(ctx, m_cntr, opt);
            } else {
                switch(m->type->category) {
                case INTRO_ARRAY: {
                    intro_print_x(ctx, m_cntr, opt);
                }break;

                case INTRO_POINTER: {
                    void * ptr = *(void **)m_data;
                    if ((m_index > 0 && m->offset == type->members[m_index - 1].offset) || m->type->of->category == INTRO_UNKNOWN) {
                        printf("0x%016x", (int)(uintptr_t)ptr);
                        break;
                    }
                    if (ptr) {
                        intro_print_x(ctx, m_cntr, opt);
                    } else {
                        printf("<null>");
                    }
                }break;

                case INTRO_STRUCT: // FALLTHROUGH
                case INTRO_UNION: {
                    IntroPrintOptions opt2 = *opt;
                    opt2.indent++;
                    intro_print_x(ctx, m_cntr, &opt2);
                }break;

                case INTRO_ENUM: {
                    intro_print_x(ctx, m_cntr, opt);
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
            if (!intro_attribute_length_x(ctx, container, &length)) {
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
            if (!intro_attribute_length_x(ctx, container, &length)) {
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
    assert(*array != NULL);
    memcpy(arraddnptr(*array, bytes), &number, bytes);
}

static uint32_t
next_uint(const uint8_t ** ptr, uint8_t size) {
    uint32_t result = 0;
    //memcpy(&result + 4 - size, *ptr, size); // BE to BE
    memcpy(&result, *ptr, size); // LE to LE
    *ptr += size;
    return result;
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

    case INTRO_UNION: {
        size_t size = 1;
        for (uint32_t i=0; i < type->count; i++) {
            size_t m_size = packed_size(city, type->members[i].type);
            if (m_size > size) size = m_size;
        }
        size += 2;
        return size;
    }break;

    case INTRO_ARRAY:
        return type->count * packed_size(city, type->of);

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
city__get_serialized_id(CityContext * city, const IntroType * type) {
    ptrdiff_t type_id_index = hmgeti(city->type_set, type);
    if (type_id_index >= 0) {
        return city->type_set[type_id_index].value;
    }

    if (intro_is_scalar(type)) {
        put_uint(&city->info, type->category, 1);
    } else {
        switch(type->category) {
        case INTRO_ARRAY: {
            uint32_t elem_type_id = city__get_serialized_id(city, type->of);
            put_uint(&city->info, type->category, 1);
            put_uint(&city->info, elem_type_id, city->type_size);
            put_uint(&city->info, type->count, city->ptr_size);
        }break;

        case INTRO_POINTER: {
            uint32_t ptr_type_id = city->type_id_counter++;
            hmput(city->type_set, type, ptr_type_id);

            put_uint(&city->info, type->category, 1);

            size_t of_type_id_index = arraddnindex(city->info, city->type_size);

            uint32_t of_type_id = city__get_serialized_id(city, type->of);
            memcpy(&city->info[of_type_id_index], &of_type_id, city->type_size);

            return ptr_type_id;
        }break;

        case INTRO_ENUM: {
            put_uint(&city->info, type->category, 1);
            put_uint(&city->info, type->size, 1);
        }break;

        case INTRO_UNION:
        case INTRO_STRUCT: {
            uint32_t * m_type_ids = (uint32_t *)malloc(type->count * sizeof(uint32_t));
            for (uint32_t m_index=0; m_index < type->count; m_index++) {
                const IntroMember * m = &type->members[m_index];
                m_type_ids[m_index] = city__get_serialized_id(city, m->type);
            }

            size_t id_test_bit = 1 << (city->ptr_size * 8 - 1);

            put_uint(&city->info, type->category, 1);
            put_uint(&city->info, type->count, city->ptr_size);
            for (uint32_t m_index=0; m_index < type->count; m_index++) {
                const IntroMember * m = &type->members[m_index];
                put_uint(&city->info, m_type_ids[m_index], city->type_size);

                int32_t id;
                if (intro_attribute_int_x(city->ictx, m->attr, city->ictx->attr.builtin.i_id, &id)) {
                    size_t stored = id;
                    stored |= id_test_bit;
                    put_uint(&city->info, stored, city->ptr_size);
                } else {
                    if (!m->name) {
                        city__error("Unnamed members must have an id.");
                        exit(1);
                    }
                    uint32_t name_offset = shget(city->name_cache, m->name);
                    if (name_offset == CITY_INVALID_CACHE) {
                        size_t m_name_len = strlen(m->name);
                        name_offset = arrlen(city->data);
                        memcpy(arraddnptr(city->data, m_name_len + 1), m->name, m_name_len + 1);
                        shput(city->name_cache, m->name, name_offset);
                    }
                    put_uint(&city->info, name_offset, city->ptr_size);
                }
            }

            free(m_type_ids);
        }break;

        default: break;
        }
    }

    uint32_t type_id = city->type_id_counter++;
    hmput(city->type_set, type, type_id);
    return type_id;
}

static void
city__serialize(CityContext * city, uint32_t data_offset, IntroContainer cont) {
    const IntroType * type = cont.type;
    const u8 * src = cont.data;

    if (!intro_has_attribute_x(city->ictx, intro_get_attr(cont), city->ictx->attr.builtin.i_city)) {
        memset(city->data + data_offset, 0, packed_size(city, type));
        return;
    }

    switch(type->category) {
    case INTRO_STRUCT: {
        uint32_t current_offset = 0;
        for (uint32_t m_index=0; m_index < type->count; m_index++) {
            city__serialize(city, data_offset + current_offset, intro_push(&cont, m_index));

            IntroMember member = type->members[m_index];
            current_offset += packed_size(city, member.type);
        }
    }break;

    case INTRO_UNION: {
        memset(city->data + data_offset, 0, packed_size(city, type));
        for (int i=0; i < type->count; i++) {
            int64_t is_valid;
            IntroContainer m_cntr = intro_push(&cont, i);
            if (intro_attribute_expr_x(city->ictx, m_cntr, city->ictx->attr.builtin.i_when, &is_valid) && is_valid) {
                uint16_t selection_index = i;
                memcpy(city->data + data_offset, &selection_index, 2);
                city__serialize(city, data_offset + 2, m_cntr);
                break;
            }
        }
    }break;

    case INTRO_POINTER: {
        if (type->of->size == 0) return;
        const u8 * ptr = *(const u8 **)src;
        if (!ptr) {
            memset(city->data + data_offset, 0, city->ptr_size);
            return;
        }

        int64_t length;
        uint32_t attr = intro_get_attr(cont);
        if (intro_attribute_length_x(city->ictx, cont, &length)) {
        } else if (intro_has_attribute_x(city->ictx, attr, city->ictx->attr.builtin.i_cstring)) {
            const char * str = (char *)ptr;
            if (str) {
                length = strlen(str) + 1;
            } else {
                length = 0;
            }
        } else {
            length = 1;
        }

        uint32_t elem_size = packed_size(city, type->of);
        uint32_t buf_size = elem_size * length;
        CityBuffer buf;
        CityDeferredPointer dptr;
        dptr.data_location = data_offset;
        for (int buf_i=0; buf_i < arrlen(city->buffers); buf_i++) {
            buf = city->buffers[buf_i];
            if (ptr == buf.origin && buf_size == buf.size) {
                dptr.ptr_value = buf.ser_offset;
                return;
            }
        }

        uint32_t * ser_length = (uint32_t *)arraddnptr(city->data, 4); // TODO: this should go with a ptr, not with the buffer
        memcpy(ser_length, &length, 4);

        uint32_t ser_offset = arraddnindex(city->data, buf_size);

        buf.origin = ptr;
        buf.ser_offset = ser_offset;
        buf.size = buf_size;
        arrput(city->buffers, buf);

        dptr.ptr_value = ser_offset;
        arrput(city->deferred_ptrs, dptr);

        if (intro_is_scalar(type->of)) {
            memcpy(city->data + ser_offset, ptr, buf_size);
        } else {
            for (uint32_t elem_i=0; elem_i < length; elem_i++) {
                uint32_t elem_offset = ser_offset + (elem_i * elem_size);
                city__serialize(city, elem_offset, intro_push(&cont, elem_i));
            }
        }
    }break;

    case INTRO_ARRAY: {
        if (intro_is_scalar(type->of)) {
            memcpy(city->data + data_offset, src, type->size);
        } else {
            size_t ser_size = packed_size(city, type->of);
            for (uint32_t elem_i=0; elem_i < type->count; elem_i++) {
                uint32_t elem_offset = data_offset + (elem_i * ser_size);
                city__serialize(city, elem_offset, intro_push(&cont, elem_i));
            }
        }
    }break;

    default: {
        memcpy(city->data + data_offset, src, type->size);
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

    CityContext city_ = {0}, *city = &city_;

    shdefault(city->name_cache, CITY_INVALID_CACHE);
    city->ictx = ictx;

    city->type_size = 2;
    city->ptr_size = 3;
    header.size_info = ((city->type_size-1) << 4) | (city->ptr_size-1);

    arraddnptr(city->data, packed_size(city, s_type));

    arrsetcap(city->info, 64);
    uint32_t main_type_id = city__get_serialized_id(city, s_type);
    uint32_t count_types = hmlenu(city->type_set);
    assert(main_type_id == count_types - 1);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arrlen(city->info);

    CityBuffer src_buf;
    src_buf.origin = (const u8 *)src;
    src_buf.ser_offset = 0;
    src_buf.size = packed_size(city, s_type);

    arrput(city->buffers, src_buf);
    city__serialize(city, 0, intro_container((void *)src, s_type));

    for (int i=0; i < arrlen(city->deferred_ptrs); i++) {
        CityDeferredPointer dptr = city->deferred_ptrs[i];
        u8 * o_ptr = city->data + dptr.data_location;
        memcpy(o_ptr, &dptr.ptr_value, city->ptr_size);
    }
    arrfree(city->deferred_ptrs);

    size_t result_size = header.data_ptr + arrlen(city->data);
    u8 * result = (u8 *)malloc(result_size);
    u8 * p = result;
    memcpy(p, &header, sizeof(header));
    p += sizeof(header);

    memcpy(p, city->info, arrlen(city->info));
    p += arrlen(city->info);

    memcpy(p, city->data, arrlen(city->data));

    arrfree(city->info);
    arrfree(city->data);
    hmfree(city->type_set);

    *o_size = result_size;
    return (void *)result;
}

static int
city__load_into(
    CityContext * city,
    IntroContainer d_cont,
    void * restrict src,
    const IntroType * restrict s_type
) {
    IntroContext * ctx = city->ictx;
    const IntroType * d_type = d_cont.type;
    u8 * dest = d_cont.data;

    uint16_t union_selection = 0;
    if (s_type->category == INTRO_UNION) {
        memcpy(&union_selection, src, 2);
        src += 2;
    }

    switch(s_type->category) {
    case INTRO_UNION:
    case INTRO_STRUCT: {
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
            IntroVariant var;
            if (intro_attribute_value_x(ctx, NULL, dm->attr, ctx->attr.builtin.i_alias, &var)) {
                char * alias = (char *)var.data;
                arrput(aliases, alias);
            }

            bool found_match = false;
            // TODO: it would probably be faster to build a hash lookup during the type parse so this isn't slow searching

            uint32_t iter_start, iter_end;
            if (s_type->category == INTRO_UNION) {
                iter_start = union_selection;
                iter_end = union_selection + 1;
            } else {
                iter_start = 0;
                iter_end = s_type->count;
            }

            for (uint32_t j = iter_start; j < iter_end; j++) {
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
                    found_match = true;
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

                    int ret = city__load_into(city,
                        intro_push(&d_cont, dm_i),
                        (u8 *)src + sm->offset, sm->type
                    );
                    if (ret < 0) return ret;
                    if (d_type->category == INTRO_UNION) {
                        return 0;
                    } else {
                        break;
                    }
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
    #if 0 // TODO...
        int32_t length_member_index;
        if (intro_attribute_member_x(ctx, dm->attr, ctx->attr.builtin.i_length, &length_member_index)) {
            const IntroMember * lm = &d_type->members[length_member_index];
            size_t wr_size = lm->type->size;
            if (wr_size > 4) wr_size = 4;
            memcpy((u8 *)dest + lm->offset, &length, wr_size);
            if ((uint32_t)length_member_index > dm_i) {
                arrput(skip_members, length_member_index);
            }
        }
    #endif
        const u8 * b = (u8 *)src;
        uintptr_t offset = next_uint(&b, city->ptr_size);
        if (offset != 0) {
            uint32_t length = 1;
            memcpy(&length, city->data + offset - 4, 4); // TODO: remove

            u8 * src_ptr = city->data + offset;

            u8 * dest_ptr = (u8 *)malloc(d_type->of->size * length); // TODO: track
            memcpy(dest, &dest_ptr, sizeof(void *));

            if (intro_is_scalar(d_type->of)) {
                memcpy(dest_ptr, src_ptr, length * d_type->of->size);
            } else {
                for (uint32_t i=0; i < length; i++) {
                    city__load_into(city, intro_push(&d_cont, i), src_ptr + (i * s_type->of->size), s_type->of);
                }
            }
        } else {
            intro_set_member_value_x(ctx, d_cont.parent->data, d_cont.parent->type, d_cont.index, ctx->attr.builtin.i_default);
        }
    }break;

    case INTRO_ARRAY: {
        for (uint32_t i=0; i < s_type->count; i++) {
            city__load_into(city, intro_push(&d_cont, i), src + (i * s_type->of->size), s_type->of);
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

    city->type_size = 1 + ((header->size_info >> 4) & 0x0f);
    city->ptr_size  = 1 + ((header->size_info) & 0x0f);

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

    MemArena * arena = new_arena(4096);

    for (uint32_t i=0; i < header->count_types; i++) {
        IntroType * type = (IntroType *)arena_alloc(arena, sizeof(*type));
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

            IntroMember * members = arena_alloc(arena, type->count * sizeof(members[0]));
            int32_t current_offset = 0;
            for (uint32_t m=0; m < type->count; m++) {
                IntroMember member = {0};
                uint32_t type_id = next_uint(&b, city->type_size);
                member.type   = hmget(info_by_id, type_id);
                member.offset = current_offset;
                if (type->category == INTRO_UNION) {
                    if (member.type->size > type->size) {
                        type->size = member.type->size;
                    }
                } else {
                    current_offset += member.type->size;
                    type->size = current_offset;
                }

                size_t next = next_uint(&b, city->ptr_size);
                if ((next & id_test_bit)) {
                    member.attr = next & (~id_test_bit); // store id directly in attr since that isn't being used for anything else
                } else {
                    member.name = (char *)(city->data + next);
                }

                members[m] = member;
            }
            type->members = members;
            if (type->category == INTRO_UNION) {
                type->size += 2;
            }
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

    int copy_result = city__load_into(city, intro_container(dest, d_type), city->data, s_type);

    hmfree(info_by_id);
    free_arena(arena);

    return copy_result;
}
