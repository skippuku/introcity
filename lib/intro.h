// MIT License
//
// Copyright (c) 2022 cyman
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef INTRO_H
#define INTRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifndef __INTRO__
#define I(...)
#endif

#ifndef INTRO_CTX
#define INTRO_CTX (&__intro_ctx)
#endif

#define ITYPE(x) (&INTRO_CTX->types[ITYPE_##x])

#ifndef INTRO_API_INLINE
#define INTRO_API_INLINE static inline
#endif

#ifndef INTRO_MAX_ATTRIBUTES
#define INTRO_MAX_ATTRIBUTES 128
#endif

#ifndef INTRO_TYPE_UNION_NAME
  #if __STDC_VERSION__ < 199901L && !defined(__cplusplus) && !defined(__GNUC__)
    #define INTRO_TYPE_UNION_NAME u
  #else
    #define INTRO_TYPE_UNION_NAME
  #endif
#endif

#if defined(__GNUC__)
  #define INTRO_ALIGN(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
  #define INTRO_ALIGN(x) __declspec(align(x))
#else
  #define INTRO_ALIGN(x)
#endif

typedef uint8_t IntroGuiColor [4] I(gui_edit_color);

typedef enum IntroCategory {
    INTRO_UNKNOWN = 0x0,

    INTRO_U8  = 0x11,
    INTRO_U16 = 0x12,
    INTRO_U32 = 0x14,
    INTRO_U64 = 0x18,

    INTRO_S8  = 0x21,
    INTRO_S16 = 0x22,
    INTRO_S32 = 0x24,
    INTRO_S64 = 0x28,

    INTRO_F32 = 0x34,
    INTRO_F64 = 0x38,
    INTRO_F128 = 0x3f,

    INTRO_ARRAY   = 0x40,
    INTRO_POINTER = 0x50,

    INTRO_ENUM    = 0x60,
    INTRO_STRUCT  = 0x70,
    INTRO_UNION   = 0x71,

    INTRO_FUNCTION = 0x80,
    INTRO_VA_LIST = 0x81,
} IntroCategory;

typedef enum {
    INTRO_UNSIGNED = 0x10,
    INTRO_SIGNED   = 0x20,
    INTRO_FLOATING = 0x30,
} IntroCategoryFlags;

typedef struct IntroLocation {
    const char * path;
    uint32_t offset;
} IntroLocation;

typedef enum IntroFlags {
    INTRO_EMBEDDED_DEFINITION = 0x01,
    // unallocated
    INTRO_EXPLICITLY_GENERATED = 0x08,
    INTRO_HAS_BODY = 0x10,
    INTRO_IS_FLAGS = 0x20,

    // reuse
    INTRO_IS_SEQUENTIAL = INTRO_HAS_BODY,
} IntroFlags;

typedef struct IntroType IntroType I(~gui_edit);

typedef uint32_t IntroAttributeInfo;

typedef struct IntroMember {
    const char * name;
    IntroType * type;
    uint32_t offset;
    IntroAttributeInfo attr;
} IntroMember;

typedef struct IntroEnumValue {
    const char * name;
    int32_t value;
    IntroAttributeInfo attr;
} IntroEnumValue;

struct IntroType {
    union {
        void * __data I(~gui_show);
        IntroType * of          I(when <-category == INTRO_ARRAY || <-category == INTRO_POINTER);
        IntroMember * members   I(length <-count, when (<-category & 0xf0) == INTRO_STRUCT);
        IntroEnumValue * values I(length <-count, when <-category == INTRO_ENUM);
        IntroType ** arg_types  I(length <-count, when <-category == INTRO_FUNCTION);
    } INTRO_TYPE_UNION_NAME;
    IntroType * parent;
    const char * name;
    uint32_t count;
    IntroAttributeInfo attr;
    uint32_t size;
    uint16_t flags I(gui_format "0x%02x");
    uint8_t align;
    uint8_t category I(gui_format "0x%02x");
};

typedef struct IntroFunction {
    const char * name;
    IntroType * type;
    IntroType * return_type;
    const char ** arg_names I(length count_args);
    IntroType ** arg_types  I(length count_args);
    IntroLocation location;
    uint32_t count_args;
    uint16_t flags;
} IntroFunction;

typedef enum IntroAttributeType {
    INTRO_AT_FLAG = 0,
    INTRO_AT_INT,
    INTRO_AT_FLOAT,
    INTRO_AT_VALUE,
    INTRO_AT_MEMBER,
    INTRO_AT_TYPE, // unimplemented
    INTRO_AT_EXPR,
    INTRO_AT_REMOVE,
    INTRO_AT_COUNT
} IntroAttributeType;

typedef struct IntroAttribute {
    const char * name;
    int attr_type;
    uint32_t type_id;
    bool propagated;
} IntroAttribute;

typedef struct IntroAttributeSpec {
    INTRO_ALIGN(16) uint32_t bitset [(INTRO_MAX_ATTRIBUTES+31) / 32];
    //uint32_t value_offsets []; // Flexible array members are not officially part of C++
} IntroAttributeSpec;

typedef struct IntroBuiltinAttributeIds {
    uint8_t id;
    uint8_t bitfield;
    uint8_t fallback;
    uint8_t length;
    uint8_t alias;
    uint8_t city;
    uint8_t cstring;
    uint8_t type;
    uint8_t when;
    uint8_t remove;

    uint8_t gui_note;
    uint8_t gui_name;
    uint8_t gui_min;
    uint8_t gui_max;
    uint8_t gui_format;
    uint8_t gui_scale;
    uint8_t gui_vector;
    uint8_t gui_color;
    uint8_t gui_show;
    uint8_t gui_edit;
    uint8_t gui_edit_color;
    uint8_t gui_edit_text;
} IntroBuiltinAttributeIds;

typedef struct IntroAttributeContext {
    IntroAttribute * available I(length count_available);
    IntroAttributeSpec * spec_buffer;
    uint32_t count_available;
    uint16_t first_flag;

    IntroBuiltinAttributeIds builtin;
} IntroAttributeContext;

typedef struct IntroMacro {
    const char * name;
    const char ** parameters I(length count_parameters);
    const char * replace;
    IntroLocation location;
    uint32_t count_parameters;
} IntroMacro;

typedef struct IntroContext {
    IntroType * types         I(length count_types);
    uint8_t * values          I(length size_values);
    IntroFunction * functions I(length count_functions);
    IntroMacro * macros       I(length count_macros);

    uint32_t count_types;
    uint32_t size_values;
    uint32_t count_functions;
    uint32_t count_macros;

    IntroAttributeContext attr; 
} IntroContext;

typedef struct IntroVariant {
    void * data;
    const IntroType * type;
} IntroVariant;

typedef struct IntroContainer {
    const struct IntroContainer * parent;
    const IntroType * type;
    uint8_t * data;
    size_t index;
} IntroContainer;

union IntroRegisterData {
    uint64_t ui;
    int64_t  si;
    float    sf;
    double   df;
};

I(apply_to (char *) (cstring))
I(apply_to (void *) (~city))

I(attribute @global (
    id:       int,
    bitfield: int,
    fallback: value(@inherit) @propagate,
    length:   expr,
    when:     expr,
    alias:    value(char *),
    city:     flag @global,
    cstring:  flag @propagate,
    type:     flag,
    remove:   __remove,
))

I(attribute gui_ (
    note:   value(char *),
    name:   value(char *), // TODO: rename to friendly, move to @global
    min:    value(@inherit) @propagate,
    max:    value(@inherit) @propagate,
    format: value(char *)   @propagate,
    scale:  float           @propagate,
    vector: flag            @propagate,
    color:  value(IntroGuiColor) @propagate,
    show:   flag @global,
    edit:   flag @global,
    edit_color: flag @propagate,
    edit_text:  flag @propagate,
))

#define intro_var_get(var, T) (assert(var.type == ITYPE(T)), *(T *)var.data)

INTRO_API_INLINE bool
intro_is_scalar(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

INTRO_API_INLINE bool
intro_is_int(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_S64);
}

INTRO_API_INLINE bool
intro_is_floating(const IntroType * type) {
    return (type->category & 0xf0) == INTRO_FLOATING;
}

INTRO_API_INLINE bool
intro_is_complex(const IntroType * type) {
    return (type->category == INTRO_STRUCT
         || type->category == INTRO_UNION
         || type->category == INTRO_ENUM);
}

INTRO_API_INLINE bool
intro_has_members(const IntroType * type) {
    return (type->category & 0xf0) == INTRO_STRUCT;
}

INTRO_API_INLINE bool
intro_has_of(const IntroType * type) {
    return type->category == INTRO_ARRAY || type->category == INTRO_POINTER;
}

INTRO_API_INLINE const IntroType *
intro_origin(const IntroType * type) {
    while (type->parent) {
        type = type->parent;
    }
    return type;
}

#define intro_has_attribute(m, a) intro_has_attribute_x(INTRO_CTX, m->attr, IATTR_##a)
INTRO_API_INLINE bool
intro_has_attribute_x(IntroContext * ctx, uint32_t attr_spec_location, uint32_t attr_id) {
    assert(attr_id < INTRO_MAX_ATTRIBUTES);
    IntroAttributeSpec * spec = ctx->attr.spec_buffer + attr_spec_location;
    uint32_t bitset_index = attr_id >> 5; 
    uint32_t bit_index = attr_id & 31;
    uint32_t attr_bit = 1 << bit_index;
    return (spec->bitset[bitset_index] & attr_bit);
}

INTRO_API_INLINE IntroContainer
intro_cntr(void * data, const IntroType * type) {
    IntroContainer cntr;
    cntr.data = (uint8_t *)data;
    cntr.type = type;
    cntr.parent = NULL;
    cntr.index = 0;
    return cntr;
}

INTRO_API_INLINE IntroContainer
intro_container(void * data, const IntroType * type) {
    return intro_cntr(data, type);
}

INTRO_API_INLINE uint32_t
intro_get_attr(IntroContainer cntr) {
    if (cntr.parent && intro_has_members(cntr.parent->type)) {
        return cntr.parent->type->members[cntr.index].attr;
    } else {
        return cntr.type->attr;
    }
}

INTRO_API_INLINE const IntroMember *
intro_get_member(IntroContainer cntr) {
    if (intro_has_members(cntr.parent->type)) {
        return &cntr.parent->type->members[cntr.index];
    } else {
        return NULL;
    }
}

typedef struct {
    int indent;
    const char * tab;
} IntroPrintOptions;

// ATTRIBUTE INFO
#define intro_attribute_value(m, a, out) intro_attribute_value_x(INTRO_CTX, m->type, m->attr, IATTR_##a, out)
bool intro_attribute_value_x(IntroContext * ctx, const IntroType * type, uint32_t attr_spec, uint32_t attr_id, IntroVariant * o_var);
#define intro_attribute_int(m, a, out) intro_attribute_int_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_int_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, int32_t * o_int);
#define intro_attribute_member(m, a, out) intro_attribute_member_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_member_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, int32_t * o_int);
#define intro_attribute_float(m, a, out) intro_attribute_float_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_float_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, float * o_float);
#define intro_attribute_length(container, out) intro_attribute_length_x(INTRO_CTX, container, out)
bool intro_attribute_length_x(IntroContext * ctx, IntroContainer cont, int64_t * o_length);
#define intro_attribute_run_expr(C, A, OUT) intro_attribute_expr_x(INTRO_CTX, C, A, OUT)
bool intro_attribute_expr_x(IntroContext * ctx, IntroContainer cntr, uint32_t attr_id, int64_t * o_result);

// INITIALIZERS
void intro_set_member_value_x(IntroContext * ctx, void * dest, const IntroType * struct_type, uint32_t member_index, uint32_t value_attribute);
#define intro_set_values(dest, type, a) intro_set_values_x(INTRO_CTX, dest, type, IATTR_##a)
void intro_set_values_x(IntroContext * ctx, void * dest, const IntroType * type, uint32_t value_attribute);
#define intro_set_defaults(dest, type) intro_set_defaults_x(INTRO_CTX, dest, type)
#define intro_default(dest, type) intro_set_defaults_x(INTRO_CTX, dest, type)
void intro_set_defaults_x(IntroContext * ctx, void * dest, const IntroType * type);

// PRINTERS
void intro_sprint_type_name(char * dest, const IntroType * type);
void intro_print_type_name(const IntroType * type);
#define intro_print(DATA, TYPE, OPT) intro_print_x(INTRO_CTX, intro_container(DATA, TYPE), OPT)
void intro_print_x(IntroContext * ctx, IntroContainer container, const IntroPrintOptions * opt);

void intro_sprint_json_x(IntroContext * ctx, char * buf, const void * data, const IntroType * type, const IntroPrintOptions * opt);

// CITY IMPLEMENTATION
char * intro_read_file(const char * filename, size_t * o_size);
int intro_dump_file(const char * filename, void * data, size_t data_size);
#define intro_load_city_file(dest, dest_type, filename) intro_load_city_file_x(INTRO_CTX, dest, dest_type, filename)
bool intro_load_city_file_x(IntroContext * ctx, void * dest, const IntroType * dest_type, const char * filename);
#define intro_create_city_file(filename, src, src_type) intro_create_city_file_x(INTRO_CTX, filename, src, src_type)
bool intro_create_city_file_x(IntroContext * ctx, const char * filename, void * src, const IntroType * src_type);
#define intro_create_city(src, s_type, o_size) intro_create_city_x(INTRO_CTX, src, s_type, o_size)
void * intro_create_city_x(IntroContext * ctx, const void * src, const IntroType * s_type, size_t *o_size);
#define intro_load_city(dest, dest_type, data, data_size) intro_load_city_x(INTRO_CTX, dest, dest_type, data, data_size)
int intro_load_city_x(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, size_t data_size);

// DEAR IMGUI (must link with intro_imgui.cpp to use)
#define intro_imgui_edit(data, data_type) intro_imgui_edit_x(INTRO_CTX, intro_container(data, data_type), #data)
void intro_imgui_edit_x(IntroContext * ctx, IntroContainer cont, const char * name);

// MISC
IntroContainer intro_push(const IntroContainer * parent, int32_t index);
IntroType * intro_type_with_name_x(IntroContext * ctx, const char * name);
const char * intro_enum_name(const IntroType * type, int value);
int64_t intro_int_value(const void * data, const IntroType * type);
#define intro_member_by_name(t, name) intro_member_by_name_x(t, #name)
const IntroMember * intro_member_by_name_x(const IntroType * type, const char * name);
union IntroRegisterData intro_run_bytecode(uint8_t * code, const void * data);

///////////////////////////////
//  INTROLIB IMPLEMENTATION  //
///////////////////////////////
#ifdef INTRO_IMPL
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
  #define INTRO_UNUSED __attribute__((unused))
#else
  #define INTRO_UNUSED
#endif

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#endif

#if defined(__x86_64__)
  #include <x86intrin.h>
  #define INTRO_POPCNT32 __popcntd
  #define INTRO_BSR64 __bsrq
#else
  #define INTRO_POPCNT32 intro_popcnt32_x
  #define INTRO_BSR64 intro_bsr64_x

  // taken from https://github.com/BartMassey/popcount/blob/master/popcount.c
  INTRO_API_INLINE int
  intro_popcnt32_x(uint32_t x) {
      const uint32_t m1 = 0x55555555;
      const uint32_t m2 = 0x33333333;
      const uint32_t m4 = 0x0f0f0f0f;
      x -= (x >> 1) & m1;
      x = (x & m2) + ((x >> 2) & m2);
      x = (x + (x >> 4)) & m4;
      x += x >>  8;
      return (x + (x >> 16)) & 0x3f;
  }

  INTRO_API_INLINE uint64_t
  intro_bsr64_x(uint64_t x) {
      uint64_t index = 63;
      for (; index > 0; index--) {
          if ((x & (1 << index))) {
              return index;
          }
      }
      return 0;
  }
#endif

#ifdef __cplusplus
  #define restrict
#endif

const static int MAX_EXPOSED_LENGTH = 64;
static const char * tab = "    ";

typedef uint8_t u8;

typedef struct {
    int current;
    int current_used;
    int capacity;
    struct {
        void * data;
    } buckets [256]; // should be enough for anyone
} MemArena;

static void *
arena_alloc(MemArena * arena, int amount) {
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
    void * result = (u8 *)arena->buckets[arena->current].data + arena->current_used;
    arena->current_used += amount;
    arena->current_used += 16 - (arena->current_used & 15);
    return result;
}

static MemArena *
new_arena(int capacity) {
    MemArena * arena = (MemArena *)calloc(1, sizeof(MemArena));
    arena->capacity = capacity;
    arena->buckets[0].data = calloc(1, arena->capacity);
    return arena;
}

static void INTRO_UNUSED
reset_arena(MemArena * arena) {
    for (int i=0; i <= arena->current; i++) {
        memset(arena->buckets[i].data, 0, arena->capacity);
    }
    arena->current = 0;
    arena->current_used = 0;
}

static void
free_arena(MemArena * arena) {
    for (uint32_t i=0; i < LENGTH(arena->buckets); i++) {
        if (arena->buckets[i].data) free(arena->buckets[i].data);
    }
    free(arena);
}

typedef struct {
    size_t cap;
    size_t len;
} DynArrayHeader;

#define arr_header(ARR) ((DynArrayHeader *)(ARR) - 1)
#define arr_init(ARR)   arr_init_((void **)&(ARR), sizeof(*(ARR)))
#define arr_free(ARR)   (free(arr_header(ARR)), ARR = 0)
#define arr_len(ARR)    arr_header(ARR)->len

#define arr_grow(ARR) arr_grow_((void **)&(ARR), sizeof(*(ARR)))
#define arr_append(ARR, VAL)              (arr_len(ARR)+=1,       arr_grow(ARR), (ARR)[arr_header(ARR)->len - 1] = VAL)
#define arr_append_range(ARR, PTR, COUNT) (arr_len(ARR)+=(COUNT), arr_grow(ARR), memcpy(&(ARR)[arr_len(ARR) - (COUNT)], PTR, (COUNT) * sizeof(*(ARR))))
#define arr_alloc_ptr(ARR, COUNT)         (arr_len(ARR)+=(COUNT), arr_grow(ARR), (ARR) + arr_len(ARR) - (COUNT))
#define arr_alloc_idx(ARR, COUNT)         (arr_len(ARR)+=(COUNT), arr_grow(ARR), arr_len(ARR) - (COUNT))

static void
arr_init_(void ** o_arr, size_t elem_size) {
#define ARR_INIT_CAP 128
    void * handle = malloc(elem_size * ARR_INIT_CAP + sizeof(DynArrayHeader));

    DynArrayHeader * header = (DynArrayHeader *)handle;
    header->cap = ARR_INIT_CAP;
    header->len = 0;

    *o_arr = (DynArrayHeader *)handle + 1;
#undef ARR_INIT_CAP
}

static void
arr_grow_(void ** o_arr, size_t elem_size) {
    bool do_realloc = false;
    while (arr_header(*o_arr)->len > arr_header(*o_arr)->cap) {
        arr_header(*o_arr)->cap <<= 1;
        do_realloc = true;
    }

    if (do_realloc) {
        void * handle = realloc(arr_header(*o_arr), arr_header(*o_arr)->cap * elem_size + sizeof(DynArrayHeader));
        *o_arr = (DynArrayHeader *)handle + 1;
    }
}

static const uint32_t MURMUR32_SEED = 0x67FD5513;

// borrowed and adapted from from gingerBill's gb.h
static uint32_t
gb_murmur32_seed(void const *data, size_t len, uint32_t seed) {
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t r1 = 15;
    const uint32_t r2 = 13;
    const uint32_t m  = 5;
    const uint32_t n  = 0xe6546b64;

    size_t count_blocks = len / 4;
    uint32_t hash = seed, k1 = 0;
    const uint32_t * blocks = (const uint32_t *)data;
    const uint8_t  * tail   = (const uint8_t  *)data + count_blocks * 4;

    for (size_t i=0; i < count_blocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
    }

    switch (len & 3) {
    case 3:
        k1 ^= tail[2] << 16;
        // FALLTHROUGH
    case 2:
        k1 ^= tail[1] << 8;
        // FALLTHROUGH
    case 1:
        k1 ^= tail[0];

        k1 *= c1;
        k1 = (k1 << r1) | (k1 >> (32 - r1));
        k1 *= c2;
        hash ^= k1;
    }

    hash ^= len;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);

    return hash;
}

static const uint32_t TABLE_INVALID_INDEX = UINT32_MAX;
static const size_t   TABLE_INVALID_VALUE = SIZE_MAX;

typedef struct {
    uint32_t hash;
    uint32_t index;
} HashLookup;

typedef struct {
    const void * key_data;
    size_t key_size;
    size_t value;
} HashEntry;

typedef struct {
    HashLookup * hashes;
    HashEntry  * entries;
    MemArena   * arena;
} HashTable;

static void
table_clear_lookups_(HashTable * tbl, size_t start, size_t end) {
    for (size_t i=start; i < end; i++) {
        tbl->hashes[i].index = TABLE_INVALID_INDEX;
    }
}

static HashTable *
new_table(size_t cap) {
    assert((size_t)1 << INTRO_BSR64(cap) == cap); // must be power of 2
    cap <<= 1;

    HashTable * tbl = (HashTable *)malloc(sizeof(*tbl));

    arr_init(tbl->hashes);
    arr_init(tbl->entries);
    (void) arr_alloc_idx(tbl->hashes, cap);

    tbl->arena = new_arena(1024);

    table_clear_lookups_(tbl, 0, cap);

    return tbl;
}

#define table_count(TBL) arr_header((TBL)->entries)->len

#define free_table(TBL) (free_table_(TBL), TBL = 0)
static void
free_table_(HashTable * tbl) {
    arr_free(tbl->hashes);
    arr_free(tbl->entries);

    free_arena(tbl->arena);

    free(tbl);
}

static void
table_insert_lookup_(HashTable * tbl, HashLookup * p_lookup) {
    uint32_t index = p_lookup->hash & (arr_len(tbl->hashes) - 1);

    HashLookup * p_slot = &tbl->hashes[index];
    while (p_slot->index != TABLE_INVALID_INDEX) {
        if (p_slot->index == p_lookup->index) {
            return;
        }
        index += 1;
        index &= arr_len(tbl->hashes) - 1;
        p_slot = &tbl->hashes[index];
    }

    *p_slot = *p_lookup;

    p_lookup->index = TABLE_INVALID_INDEX;
}

static void
table_grow_lookup_(HashTable * tbl) {
    size_t old_len = arr_len(tbl->hashes);
    arr_header(tbl->hashes)->len <<= 1;
    arr_grow(tbl->hashes);

    table_clear_lookups_(tbl, old_len, arr_len(tbl->hashes));

    for (size_t i=0; i < old_len; i++) {
        HashLookup * p_lookup = &tbl->hashes[i];
        if (p_lookup->index != TABLE_INVALID_INDEX) {
            table_insert_lookup_(tbl, p_lookup);
        }
    }
}

static uint32_t
table_get(const HashTable * tbl, HashEntry * p_entry) {
    uint32_t hash = gb_murmur32_seed(p_entry->key_data, p_entry->key_size, MURMUR32_SEED);
    uint32_t lookup_index_mask = arr_len(tbl->hashes) - 1;
    uint32_t lookup_index = hash & lookup_index_mask;

    while (1) {
        HashLookup lookup = tbl->hashes[lookup_index];
        if (lookup.index == TABLE_INVALID_INDEX) {
            p_entry->value = TABLE_INVALID_VALUE;
            return TABLE_INVALID_INDEX;
        }
        if (lookup.hash == hash) {
            HashEntry s_entry = tbl->entries[lookup.index];
            if (
                s_entry.key_size == p_entry->key_size
             && 0==memcmp(s_entry.key_data, p_entry->key_data, s_entry.key_size)
               )
            {
                p_entry->value = s_entry.value;
                return lookup.index;
            }
        }
        lookup_index += 1;
        lookup_index &= lookup_index_mask;
    }
}

static void
table_set(HashTable * tbl, HashEntry entry) {
    size_t new_value = entry.value;
    uint32_t entry_index = table_get(tbl, &entry);
    if (entry_index != TABLE_INVALID_INDEX) {
        tbl->entries[entry_index].value = new_value;
        return;
    }

    void * stored_key = arena_alloc(tbl->arena, entry.key_size);
    memcpy(stored_key, entry.key_data, entry.key_size);
    entry.key_data = stored_key;
    entry.value = new_value;

    entry_index = arr_len(tbl->entries);
    arr_append(tbl->entries, entry);

    if (arr_len(tbl->entries) > arr_len(tbl->hashes) / 2) {
        table_grow_lookup_(tbl);
    }

    HashLookup lookup;
    lookup.hash = gb_murmur32_seed(entry.key_data, entry.key_size, MURMUR32_SEED);
    lookup.index = entry_index;

    table_insert_lookup_(tbl, &lookup);
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
        pop += INTRO_POPCNT32(spec->bitset[i]);
    }
    pop += INTRO_POPCNT32(spec->bitset[bitset_index] & pop_count_mask);
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
    return intro_attribute_expr_x(ctx, cntr, ctx->attr.builtin.length, o_length);
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

#ifndef INTRO_USE_ASM_VM
#define INTRO_USE_ASM_VM 0
#endif

typedef enum {
    I_INVALID = 0,
    I_RETURN = 1,
    I_LD8,
    I_LD16,
    I_LD32,
    I_LD64,
    I_IMM8,
    I_IMM16,
    I_IMM32,
    I_IMM64,
    I_ZERO,
    I_CND_LD_TOP,
    I_NEGATE_I,
    I_NEGATE_F,
    I_BIT_NOT,
    I_BOOL,
    I_BOOL_NOT,
    I_SETL,
    I_SETE,
    I_SETLE,
    I_CVT_D_TO_I,
    I_CVT_F_TO_I,
    I_CVT_I_TO_D,
    I_CVT_F_TO_D,    I_GREATER_POP = I_CVT_F_TO_D,
    I_ADDI,
    I_MULI,
    I_DIVI,
    I_MODI,
    I_L_SHIFT,
    I_R_SHIFT,
    I_BIT_AND,
    I_BIT_OR,
    I_BIT_XOR,
    I_CMP,
    I_CMP_F,
    I_ADDF,
    I_MULF,
    I_DIVF,

    I_COUNT
} InstrCode;

union IntroRegisterData
intro_run_bytecode(uint8_t * code, const void * v_data) {
#if INTRO_USE_ASM_VM == 0
    const uint8_t * data = (uint8_t *)v_data;
    union IntroRegisterData stack [1024];
    union IntroRegisterData r0, r1, r2;
    size_t stack_idx = 0;
    size_t code_idx = 0;
    bool flag_l = 0, flag_e = 0;

    memset(&r1, 0, sizeof(r1)); // silence dumb warning

    while (1) {
        uint8_t byte = code[code_idx++];
        uint8_t inst = byte;

        if (inst > I_GREATER_POP) {
            r1 = r0;
            r0 = stack[--stack_idx];
        }

        switch ((InstrCode)inst) {
        case I_RETURN: return r0;

        case I_LD8 : r0.ui = *(uint8_t  *)(data + r0.ui); break;
        case I_LD16: r0.ui = *(uint16_t *)(data + r0.ui); break;
        case I_LD32: r0.ui = *(uint32_t *)(data + r0.ui); break;
        case I_LD64: r0.ui = *(uint64_t *)(data + r0.ui); break;

        case I_IMM8:  stack[stack_idx++] = r0;
                      r0.ui = 0;
                      r0.ui = code[code_idx];
                      code_idx += 1;
                      break;
        case I_IMM16: stack[stack_idx++] = r0;
                      r0.ui = 0;
                      memcpy(&r0, code + code_idx, 2);
                      code_idx += 2;
                      break;
        case I_IMM32: stack[stack_idx++] = r0;
                      r0.ui = 0;
                      memcpy(&r0, code + code_idx, 4);
                      code_idx += 4;
                      break;
        case I_IMM64: stack[stack_idx++] = r0;
                      memcpy(&r0, code + code_idx, 8);
                      code_idx += 8;
                      break;
        case I_ZERO:  stack[stack_idx++] = r0;
                      r0.ui = 0;
                      break;

        case I_CND_LD_TOP: r1 = stack[--stack_idx]; // alternate value
                           r2 = stack[--stack_idx]; // condition
                           if (r2.ui) r0 = r1;
                           break;

        case I_NEGATE_I:   r0.si = -r0.si; break;
        case I_NEGATE_F:   r0.df = -r0.df; break;
        case I_BIT_NOT:    r0.ui = ~r0.ui; break;
        case I_BOOL:       r0.ui = !!r0.ui; break;
        case I_BOOL_NOT:   r0.ui = ! r0.ui; break;
        case I_SETE:       r0.ui = flag_e; break;
        case I_SETL:       r0.ui = flag_l; break;
        case I_SETLE:      r0.ui = flag_e || flag_l; break;
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

        case I_CMP:   flag_l = r0.si < r1.si;
                      flag_e = r0.si == r1.si;
                      break;
        case I_CMP_F: flag_l = r0.df < r1.df;
                      flag_e = r0.df == r1.df;
                      break;

        case I_ADDF: r0.df += r1.df; break;
        case I_MULF: r0.df *= r1.df; break;
        case I_DIVF: r0.df /= r1.df; break;

        case I_COUNT: case I_INVALID: assert(0);
        }
    }
#else
    extern  __attribute__((sysv_abi)) int64_t intro__vm(void * bytecode, const void * data);

    union IntroRegisterData reg;
    reg.si = intro__vm(code, v_data);
    return reg;
#endif
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
    if (intro_has_attribute_x(ctx, m->attr, ctx->attr.builtin.type)) {
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
    intro_set_values_x(ctx, dest, type, ctx->attr.builtin.fallback);
}

void
intro_sprint_type_name(char * dest, const IntroType * type) {
    while (1) {
        if (type->category == INTRO_POINTER) {
            *dest++ = '*';
            type = type->of;
        } else if (type->category == INTRO_ARRAY) {
            dest += sprintf(dest, "[%u]", type->count);
            type = type->of;
        } else if (type->name) {
            dest += sprintf(dest, "%s", type->name);
            break;
        } else {
            dest += sprintf(dest, "<anon>");
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
    IntroPrintOptions opt_default;
    const IntroType * type = container.type;
    const void * data = container.data;

    if (!opt) {
        memset(&opt_default, 0, sizeof(opt_default));
        opt_default.tab = "    ";
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
              ||(intro_attribute_expr_x(ctx, m_cntr, ctx->attr.builtin.when, &expr_result) && !expr_result)
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
        } else if (intro_has_attribute_x(ctx, attr, ctx->attr.builtin.cstring)) {
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
        if (parent->data) {
            result.data = *(uint8_t **)(parent->data) + result.type->size * index;
        } else {
            result.data = NULL;
        }
    }break;

    case INTRO_ARRAY: {
        result.type = parent->type->of;
        if (parent->data) {
            result.data = parent->data + result.type->size * index;
        } else {
            result.data = NULL;
        }
    }break;

    case INTRO_STRUCT:
    case INTRO_UNION: {
        result.type = parent->type->members[index].type;
        if (parent->data) {
            result.data = parent->data + parent->type->members[index].offset;
        } else {
            result.data = NULL;
        }
    }break;

    default:
        result = *parent;
        return result;
    }

    result.index = index;
    result.parent = parent;
    return result;
}

// JSON GENERATION

#define DO_INDENT(OPT) for (int _i=0; _i < (OPT)->indent; _i++) *p_out += sprintf(*p_out, "%s", (OPT)->tab)

static void intro_generate_json_internal(IntroContext * ctx, char ** p_out, IntroContainer cntr, IntroPrintOptions * opt);

static void
intro_generate_json_array_internal(IntroContext * ctx, char ** p_out, IntroContainer cntr, size_t count, IntroPrintOptions * opt) {
    bool do_newlines = !intro_is_scalar(cntr.type->of);
    char space = (do_newlines)? '\n' : ' ';

    IntroPrintOptions n_opt = *opt;
    if (do_newlines) {
        n_opt.indent += 1;
    }

    *p_out += sprintf(*p_out, "[%c", space);

    for (size_t elem_i=0; elem_i < count; elem_i++) {
        if (do_newlines) {
            DO_INDENT(&n_opt);
        }
        intro_generate_json_internal(ctx, p_out, intro_push(&cntr, elem_i), &n_opt);
        if (elem_i < count - 1) {
            *p_out += sprintf(*p_out, ",%c", space);
        }
    }

    *p_out += sprintf(*p_out, "%c", space);
    if (do_newlines) {
        DO_INDENT(opt);
    }
    *p_out += sprintf(*p_out, "]");
}

static void
intro_generate_json_internal(IntroContext * ctx, char ** p_out, IntroContainer cntr, IntroPrintOptions * opt) {
    switch (cntr.type->category) {
    case INTRO_U8: {
        const IntroType * origin = intro_origin(cntr.type);
        uint8_t value = *(uint8_t *)cntr.data;
        if (origin->name && 0==strcmp(origin->name, "bool")) {
            *p_out += sprintf(*p_out, "%s", value? "true" : "false");
        } else {
            *p_out += sprintf(*p_out, "%u", (unsigned int)value);
        }
    }break;

    case INTRO_U16: case INTRO_U32: case INTRO_U64:
    case INTRO_S8: case INTRO_S16: case INTRO_S32: case INTRO_S64: {
        ssize_t value = intro_int_value(cntr.data, cntr.type);
        *p_out += sprintf(*p_out, "%li", (long int)value);
    }break;

    case INTRO_F32: {
        *p_out += sprintf(*p_out, "%g", *(float *)cntr.data);
    }break;

    case INTRO_F64: {
        *p_out += sprintf(*p_out, "%g", *(double *)cntr.data);
    }break;

    case INTRO_STRUCT: {
        *p_out += sprintf(*p_out, "{\n");
        for (size_t member_i=0; member_i < cntr.type->count; member_i++) {
            IntroContainer m_cntr = intro_push(&cntr, member_i);
            IntroPrintOptions m_opt = *opt;
            m_opt.indent += 1;

            DO_INDENT(&m_opt);
            *p_out += sprintf(*p_out, "\"%s\" : ", intro_get_member(m_cntr)->name);

            intro_generate_json_internal(ctx, p_out, m_cntr, &m_opt);

            if (member_i < cntr.type->count - 1) {
                *p_out += sprintf(*p_out, ",");
            }
            *p_out += sprintf(*p_out, "\n");
        }
        DO_INDENT(opt);
        *p_out += sprintf(*p_out, "}");
    }break;

    case INTRO_UNION: {
        IntroContainer m_cntr;

        for (uint32_t member_i=0; member_i < cntr.type->count; member_i++) {
            m_cntr = intro_push(&cntr, member_i);
            int64_t res;
            if (intro_attribute_expr_x(ctx, m_cntr, ctx->attr.builtin.when, &res) && res) {
                IntroPrintOptions n_opt = *opt;
                n_opt.indent += 1;

                char type_buf [1024];
                intro_sprint_type_name(type_buf, intro_get_member(m_cntr)->type);
                *p_out += sprintf(*p_out, "{ \"type\" : \"%s\", \"content\" : ", type_buf);
                intro_generate_json_internal(ctx, p_out, m_cntr, &n_opt);
                *p_out += sprintf(*p_out, " }");
                return;
            }
        }

        *p_out += sprintf(*p_out, "null");
    }break;

    case INTRO_ENUM: {
        int value = *(int *)cntr.data;
        *p_out += sprintf(*p_out, "%i", value);
    }break;

    case INTRO_POINTER: {
        void * ptr = *(void **)cntr.data;
        // check for circular reference
        {
            const IntroContainer * super = &cntr;
            while (super->parent) {
                super = super->parent;
                if (super->data == cntr.data) {
                    *p_out += sprintf(*p_out, "\"<circular>\"");
                    return;
                }
            }
        }
        if (!ptr) {
            *p_out += sprintf(*p_out, "null");
        } else if (intro_has_attribute_x(ctx, intro_get_attr(cntr), ctx->attr.builtin.cstring)) {
            *p_out += sprintf(*p_out, "\"%s\"", (char *)ptr);
        } else {
            int64_t length;
            if (intro_attribute_length_x(ctx, cntr, &length)) {
                intro_generate_json_array_internal(ctx, p_out, cntr, length, opt);
            } else {
                intro_generate_json_internal(ctx, p_out, intro_push(&cntr, 0), opt);
            }
        }
    }break;

    case INTRO_ARRAY: {
        int64_t length;
        if (!intro_attribute_length_x(ctx, cntr, &length)) {
            length = cntr.type->count;
        }
        intro_generate_json_array_internal(ctx, p_out, cntr, length, opt);
    }break;
    }
}

void
intro_sprint_json_x(IntroContext * ctx, char * buf, const void * data, const IntroType * type, const IntroPrintOptions * opt) {
    IntroPrintOptions n_opt;
    if (!opt) {
        memset(&n_opt, 0, sizeof(n_opt));
        n_opt.indent = 0;
        n_opt.tab = "  ";
    } else {
        n_opt = *opt;
    }

    intro_generate_json_internal(ctx, &buf, intro_cntr((void *)data, type), &n_opt);
}

#undef DO_INDENT

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
put_uint(uint8_t ** o_array, uint32_t number, uint8_t bytes) {
    assert(*o_array != NULL);
    arr_append_range(*o_array, &number, bytes);
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
    HashTable * type_set;
    CityBuffer * buffers;
    CityDeferredPointer * deferred_ptrs;
    HashTable * name_cache;
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

static uint32_t
city__get_serialized_id(CityContext * city, const IntroType * type) {
    HashEntry entry;
    entry.key_data = &type;
    entry.key_size = sizeof(type);

    table_get(city->type_set, &entry);
    size_t type_id_index = entry.value;
    if (type_id_index != TABLE_INVALID_VALUE) {
        return entry.value;
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

            entry.value = ptr_type_id;
            table_set(city->type_set, entry);

            put_uint(&city->info, type->category, 1);

            size_t of_type_id_index = arr_alloc_idx(city->info, city->type_size);

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
                if (intro_attribute_int_x(city->ictx, m->attr, city->ictx->attr.builtin.id, &id)) {
                    size_t stored = id;
                    stored |= id_test_bit;
                    put_uint(&city->info, stored, city->ptr_size);
                } else {
                    if (!m->name) {
                        city__error("Unnamed members must have an id.");
                        exit(1);
                    }

                    size_t m_name_len = strlen(m->name);

                    HashEntry h_entry;
                    h_entry.key_data = m->name;
                    h_entry.key_size = m_name_len;

                    table_get(city->name_cache, &h_entry);
                    size_t name_offset = h_entry.value;
                    if (name_offset == TABLE_INVALID_VALUE) {
                        name_offset = arr_len(city->data);
                        arr_append_range(city->data, m->name, m_name_len + 1);

                        h_entry.value = name_offset;
                        table_set(city->name_cache, h_entry);
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
    entry.value = type_id;
    table_set(city->type_set, entry);
    return type_id;
}

static void
city__serialize(CityContext * city, uint32_t data_offset, IntroContainer cont) {
    const IntroType * type = cont.type;
    const u8 * src = cont.data;

    if (!intro_has_attribute_x(city->ictx, intro_get_attr(cont), city->ictx->attr.builtin.city)) {
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
        for (uint32_t i=0; i < type->count; i++) {
            int64_t is_valid;
            IntroContainer m_cntr = intro_push(&cont, i);
            if (intro_attribute_expr_x(city->ictx, m_cntr, city->ictx->attr.builtin.when, &is_valid) && is_valid) {
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
        } else if (intro_has_attribute_x(city->ictx, attr, city->ictx->attr.builtin.cstring)) {
            length = strlen((char *)ptr) + 1;
        } else {
            length = 1;
        }

        uint32_t elem_size = packed_size(city, type->of);
        uint32_t buf_size = elem_size * length;
        CityBuffer buf;
        CityDeferredPointer dptr;
        dptr.data_location = data_offset;
        for (size_t buf_i=0; buf_i < arr_len(city->buffers); buf_i++) {
            buf = city->buffers[buf_i];
            if (ptr == buf.origin && buf_size == buf.size) {
                dptr.ptr_value = buf.ser_offset;
                return;
            }
        }

        uint32_t * ser_length = (uint32_t *)arr_alloc_ptr(city->data, 4); // TODO: this should go with a ptr, not with the buffer
        memcpy(ser_length, &length, 4);

        uint32_t ser_offset = arr_alloc_idx(city->data, buf_size);

        buf.origin = ptr;
        buf.ser_offset = ser_offset;
        buf.size = buf_size;
        arr_append(city->buffers, buf);

        dptr.ptr_value = ser_offset;
        arr_append(city->deferred_ptrs, dptr);

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
    assert(s_type->category == INTRO_STRUCT); // TODO: remove

    // init context
    CityContext _city, * city = &_city;

    CityHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic_number, "ICTY", 4);
    header.version_major = implementation_version_major;
    header.version_minor = implementation_version_minor;

    memset(city, 0, sizeof(_city));

    city->ictx = ictx;

    city->type_size = 2;
    city->ptr_size = 3;
    header.size_info = ((city->type_size-1) << 4) | (city->ptr_size-1);

    arr_init(city->data);
    arr_init(city->info);
    arr_init(city->deferred_ptrs);
    arr_init(city->buffers);
    city->name_cache = new_table(128);
    city->type_set = new_table(128);

    // reserve space for main data
    (void) arr_alloc_idx(city->data, packed_size(city, s_type));

    // create type info

    uint32_t main_type_id = city__get_serialized_id(city, s_type);
    uint32_t count_types = table_count(city->type_set);
    assert(main_type_id == count_types - 1);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arr_len(city->info);

    // serialized data

    CityBuffer src_buf;
    src_buf.origin = (const u8 *)src;
    src_buf.ser_offset = 0;
    src_buf.size = packed_size(city, s_type);

    arr_append(city->buffers, src_buf);
    city__serialize(city, 0, intro_container((void *)src, s_type));

    for (size_t i=0; i < arr_len(city->deferred_ptrs); i++) {
        CityDeferredPointer dptr = city->deferred_ptrs[i];
        u8 * o_ptr = city->data + dptr.data_location;
        memcpy(o_ptr, &dptr.ptr_value, city->ptr_size);
    }
    arr_free(city->deferred_ptrs);

    size_t result_size = header.data_ptr + arr_len(city->data);
    u8 * result = (u8 *)malloc(result_size);
    u8 * p = result;
    memcpy(p, &header, sizeof(header));
    p += sizeof(header);

    memcpy(p, city->info, arr_len(city->info));
    p += arr_len(city->info);

    memcpy(p, city->data, arr_len(city->data));

    arr_free(city->info);
    arr_free(city->data);
    free_table(city->name_cache);
    free_table(city->type_set);

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
        src = (u8 *)src + 2;
    }

    switch(s_type->category) {
    case INTRO_UNION:
    case INTRO_STRUCT: {
        const char ** aliases = NULL;
        arr_init(aliases);

        for (uint32_t dm_i=0; dm_i < d_type->count; dm_i++) {
#if 0
            bool do_skip = false;
            for (int skip_i=0; skip_i < arr_len(skip_members); skip_i++) {
                if (skip_members[skip_i] == dm_i) {
                    uint32_t last = skip_members[--arr_len(skip_members)];
                    if (arr_len(skip_members) >= 1) {
                        skip_members[dm_i] = last;
                    }
                    do_skip = true;
                    break;
                }
            }
            if (do_skip) continue;
#endif
            const IntroMember * dm = &d_type->members[dm_i];

            if (intro_has_attribute_x(ctx, dm->attr, ctx->attr.builtin.type)) {
                *(const IntroType **)((u8 *)dest + dm->offset) = d_type;
                continue;
            }

            arr_append(aliases, dm->name);
            IntroVariant var;
            if (intro_attribute_value_x(ctx, NULL, dm->attr, ctx->attr.builtin.alias, &var)) {
                char * alias = (char *)var.data;
                arr_append(aliases, alias);
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
                    for (size_t alias_i=0; alias_i < arr_len(aliases); alias_i++) {
                        if (strcmp(aliases[alias_i], sm->name) == 0) {
                            match = true;
                            break;
                        }
                    }
                } else {
                    int32_t sm_id = sm->attr;
                    int32_t dm_id;
                    if (intro_attribute_int_x(ctx, dm->attr, ctx->attr.builtin.id, &dm_id) && dm_id == sm_id) {
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
                        snprintf(msg, sizeof(msg), "type mismatch. from: %s to: %s", from, to);
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
                intro_set_member_value_x(ctx, dest, d_type, dm_i, ctx->attr.builtin.fallback);
            }
            arr_header(aliases)->len = 0;
        }
        arr_free(aliases);
    }break;

    case INTRO_POINTER: {
    #if 0 // TODO...
        int32_t length_member_index;
        if (intro_attribute_member_x(ctx, dm->attr, ctx->attr.builtin.length, &length_member_index)) {
            const IntroMember * lm = &d_type->members[length_member_index];
            size_t wr_size = lm->type->size;
            if (wr_size > 4) wr_size = 4;
            memcpy((u8 *)dest + lm->offset, &length, wr_size);
            if ((uint32_t)length_member_index > dm_i) {
                arr_append(skip_members, length_member_index);
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
            intro_set_member_value_x(ctx, d_cont.parent->data, d_cont.parent->type, d_cont.index, ctx->attr.builtin.fallback);
        }
    }break;

    case INTRO_ARRAY: {
        for (uint32_t i=0; i < s_type->count; i++) {
            city__load_into(city, intro_push(&d_cont, i), (u8 *)src + (i * s_type->of->size), s_type->of);
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
    CityContext _city, * city = &_city;
    memset(city, 0, sizeof(_city));
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

    city->data = (uint8_t *)data + header->data_ptr;
    const uint8_t * b = (u8 *)data + sizeof(*header);

    size_t id_test_bit = 1 << (city->ptr_size * 8 - 1);

    typedef struct {
        IntroType * type;
        uint32_t of_id;
    } TypePtrOf;
    TypePtrOf * deferred_pointer_ofs = NULL;
    arr_init(deferred_pointer_ofs);

    IntroType ** info_by_id;
    arr_init(info_by_id);

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

            IntroMember * members = (IntroMember *)arena_alloc(arena, type->count * sizeof(members[0]));
            int32_t current_offset = 0;
            for (uint32_t m=0; m < type->count; m++) {
                IntroMember member;
                memset(&member, 0, sizeof(member));

                uint32_t type_id = next_uint(&b, city->type_size);
                member.type   = info_by_id[type_id];
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

            arr_append(deferred_pointer_ofs, ptrof);

            type->size = city->ptr_size;
        }break;

        case INTRO_ARRAY: {
            uint32_t elem_id = next_uint(&b, city->type_size);
            uint32_t count = next_uint(&b, city->ptr_size);

            IntroType * elem_type = info_by_id[elem_id];
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
        arr_append(info_by_id, type);
    }

    for (size_t i=0; i < arr_len(deferred_pointer_ofs); i++) {
        TypePtrOf ptrof = deferred_pointer_ofs[i];
        ptrof.type->of = info_by_id[ptrof.of_id];
    }
    arr_free(deferred_pointer_ofs);

    const IntroType * s_type = info_by_id[arr_len(info_by_id) - 1];

    int copy_result = city__load_into(city, intro_container(dest, d_type), city->data, s_type);

    arr_free(info_by_id);
    free_arena(arena);

    return copy_result;
}
#endif // INTRO_IMPL

//////////////////////////////////////////
//  INTRO IMGUI UTILITY IMPLEMENTATION  //
//////////////////////////////////////////
#ifdef INTRO_IMGUI_IMPL
#ifndef __cplusplus
  #error "intro.h must be compiled as C++ when INTRO_IMGUI_IMPL is defined"
#endif

#ifndef IMGUI_VERSION
  #error "imgui.h must be included before intro.h when INTRO_IMGUI_IMPL is defined"
#endif

#define GUIATTR(x) (ctx->attr.builtin.gui_##x)

namespace intro {

static const ImVec4 ptr_color     = ImVec4(0.9, 0.9, 0.2, 1.0);
static const ImVec4 struct_color  = ImVec4(0.2, 0.9, 0.2, 1.0);
static const ImVec4 array_color   = ImVec4(0.8, 0.1, 0.3, 1.0);
static const ImVec4 default_color = ImVec4(1.0, 1.0, 1.0, 1.0);
static const ImVec4 enum_color    = ImVec4(0.9, 0.7, 0.1, 1.0);

static int
imgui_scalar_type(const IntroType * type) {
    switch(type->category) {
    case INTRO_U8:  return ImGuiDataType_U8;
    case INTRO_S8:  return ImGuiDataType_S8;
    case INTRO_U16: return ImGuiDataType_U16;
    case INTRO_S16: return ImGuiDataType_S16;
    case INTRO_U32: return ImGuiDataType_U32;
    case INTRO_S32: return ImGuiDataType_S32;
    case INTRO_U64: return ImGuiDataType_U64;
    case INTRO_S64: return ImGuiDataType_S64;
    case INTRO_F32: return ImGuiDataType_Float;
    case INTRO_F64: return ImGuiDataType_Double;
    default: return 0;
    }
}

static void edit_member(IntroContext *, const char *, IntroContainer, int);

static void
edit_array(IntroContext * ctx, const IntroContainer * p_cont, int count) {
    for (int i=0; i < count; i++) {
        char name [64];
        snprintf(name, 63, "[%i]", i);
        edit_member(ctx, name, intro_push(p_cont, i), i);
    }
}

struct IntroImGuiScalarParams {
    float scale;
    void * min, * max;
    const char * format;
};

static IntroImGuiScalarParams
get_scalar_params(IntroContext * ctx, const IntroType * type, uint32_t attr) {
    IntroImGuiScalarParams result = {};
    result.scale = 1.0f;
    intro_attribute_float_x(ctx, attr, GUIATTR(scale), &result.scale);
            
    IntroVariant max_var = {0}, min_var = {0}, var;
    intro_attribute_value_x(ctx, type, attr, GUIATTR(min), &min_var);
    intro_attribute_value_x(ctx, type, attr, GUIATTR(max), &max_var);
    result.min = min_var.data;
    result.max = max_var.data;
    result.format = (intro_attribute_value_x(ctx, NULL, attr, GUIATTR(format), &var))? (const char *)var.data : NULL;

    return result;
}

static void
do_note(const char * note) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(note);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static ImU32
color_from_var(const void * in) {
    auto buf = reinterpret_cast<const uint8_t *>(in);
    return IM_COL32(buf[0], buf[1], buf[2], buf[3]);
}

static void
edit_member(IntroContext * ctx, const char * name, IntroContainer cont, int id) {
    const IntroType * type = cont.type;
    const IntroMember * m = NULL;
    if (cont.parent && intro_has_members(cont.parent->type)) {
        m = &cont.parent->type->members[cont.index];
    }
    uint32_t attr = (m)? m->attr : type->attr;

    int64_t expr_result;
    if (
        !intro_has_attribute_x(ctx, attr, GUIATTR(show))
      ||(intro_attribute_expr_x(ctx, cont, ctx->attr.builtin.when, &expr_result) && !expr_result)
       )
    {
        return;
    }

    ImGui::PushID(id);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    int tree_flags = ImGuiTreeNodeFlags_SpanFullWidth;
    bool has_children = type->category == INTRO_STRUCT
                     || type->category == INTRO_UNION
                     || type->category == INTRO_ARRAY
                     || type->category == INTRO_POINTER;
    if (!has_children) {
        tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    IntroVariant colorv;
    ImU32 member_color = 0xffffffff;
    if (m && intro_attribute_value_x(ctx, type, m->attr, GUIATTR(color), &colorv)) {
        member_color = color_from_var(colorv.data);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, member_color);
    bool is_open = ImGui::TreeNodeEx((name)? name : "<anon>", tree_flags);
    ImGui::PopStyleColor();

    char type_buf [1024];
    intro_sprint_type_name(type_buf, type);

    ImVec4 type_color;
    if (intro_attribute_value_x(ctx, type, type->attr, GUIATTR(color), &colorv)) {
        type_color = ImColor(color_from_var(colorv.data)).Value;
    } else {
        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: type_color = struct_color; break;
        case INTRO_ENUM: type_color = enum_color; break;
        case INTRO_POINTER: type_color = ptr_color; break;
        case INTRO_ARRAY:   type_color = array_color; break;
        default: type_color = default_color; break;
        }
    }

    // drag drop

    if (ImGui::BeginDragDropSource()) {
        IntroVariant var;
        var.data = cont.data;
        var.type = type;
        ImGui::SetDragDropPayload("IntroVariant", &var, sizeof(IntroVariant));

        ImGui::TextColored(type_color, "%s", type_buf);

        ImGui::EndDragDropSource();
    }

    const ImGuiPayload * payload = ImGui::GetDragDropPayload();
    if (payload && payload->IsDataType("IntroVariant")) {
        auto var = (const IntroVariant *)payload->Data;
        if (var->type == type) {
            ImGui::SameLine(); ImGui::TextColored(ptr_color, "[ ]");
            if (ImGui::BeginDragDropTarget()) {
                if (ImGui::AcceptDragDropPayload("IntroVariant")) {
                    memcpy(cont.data, var->data, type->size);
                }
                
                ImGui::EndDragDropTarget();
            }
        }
    }

    const char * note = NULL;
    IntroVariant var;
    if (m && intro_attribute_value_x(ctx, NULL, m->attr, GUIATTR(note), &var)) {
        note = (char *)var.data;
        do_note(note);
    }

    int64_t length = -1;
    bool has_length = false;
    if (m && intro_attribute_length_x(ctx, cont, &length)) {
        has_length = true;
    }

    ImGui::TableNextColumn();
    if (has_length) {
        ImGui::TextColored(type_color, "%s (%li)", type_buf, (long int)length);
    } else {
        ImGui::TextColored(type_color, "%s", type_buf);
    }
    if (intro_attribute_value_x(ctx, NULL, type->attr, GUIATTR(note), &var)) {
        note = (char *)var.data;
        do_note(note);
    }

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);

    bool do_tree_place_holder = true;
    if (intro_has_attribute_x(ctx, attr, GUIATTR(edit_color))) {
        size_t size = type->size;
        switch(size) {
        case 12:
            ImGui::ColorEdit3("##", (float *)cont.data);
            break;
        case 16:
            ImGui::ColorEdit4("##", (float *)cont.data);
            break;
        case 4: {
            float im_color [4];
            uint8_t * buf = (uint8_t *)cont.data;
            im_color[0] = buf[0] / 256.0f;
            im_color[1] = buf[1] / 256.0f;
            im_color[2] = buf[2] / 256.0f;
            im_color[3] = buf[3] / 256.0f;

            ImGui::ColorEdit4("##", im_color);

            buf[0] = im_color[0] * 256;
            buf[1] = im_color[1] * 256;
            buf[2] = im_color[2] * 256;
            buf[3] = im_color[3] * 256;
        }break;

        default:
            ImGui::Text("bad color size");
            break;
        }
        do_tree_place_holder = false;
    }

    if (intro_has_attribute_x(ctx, attr, GUIATTR(vector))) {
        int count_components = 0;
        const IntroType * scalar_type;
        if (type->category == INTRO_ARRAY) {
            scalar_type = type->of;
            count_components = type->count;
        } else if (intro_has_members(type)) {
            const IntroType * m_type = type->members[0].type;
            scalar_type = m_type;
            count_components = type->size / m_type->size;
        }
        if (count_components > 0) {
            auto param = get_scalar_params(ctx, type, attr);
            ImGui::DragScalarN("##", imgui_scalar_type(scalar_type), cont.data, count_components, param.scale, param.min, param.max, param.format);
            do_tree_place_holder = false;
        }
    }

    if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        if (do_tree_place_holder) ImGui::TextDisabled("---");
        if (is_open) {
            for (uint32_t m_index=0; m_index < cont.type->count; m_index++) {
                const IntroMember * m = &cont.type->members[m_index];
                edit_member(ctx, m->name, intro_push(&cont, m_index), m_index);
            }
            ImGui::TreePop();
        }
    } else if (intro_is_scalar(type)) {
        if (type->name && strcmp(type->name, "bool") == 0) {
            ImGui::Checkbox("##", (bool *)cont.data);
        } else {
            auto param = get_scalar_params(ctx, type, attr);

            ImGui::DragScalar("##", imgui_scalar_type(type), cont.data, param.scale, param.min, param.max, param.format);
        }
    } else if (type->category == INTRO_ENUM) {
        if ((type->flags & INTRO_IS_FLAGS)) {
            int * flags_ptr = (int *)cont.data;
            for (uint32_t e=0; e < type->count; e++) {
                IntroEnumValue v = type->values[e];
                ImGui::CheckboxFlags(v.name, flags_ptr, v.value);
            }
        } else {
            int current_value = *(int *)cont.data;
            bool found_match = false;
            uint32_t current_index;
            for (uint32_t e=0; e < type->count; e++) {
                IntroEnumValue v = type->values[e];
                if (v.value == current_value) {
                    current_index = e;
                    found_match = true;
                }
            }
            if (!found_match) {
                ImGui::InputInt(NULL, (int *)cont.data);
            } else {
                const char * preview = type->values[current_index].name;
                if (ImGui::BeginCombo("##", preview)) {
                    for (uint32_t e=0; e < type->count; e++) {
                        IntroEnumValue v = type->values[e];
                        bool is_selected = (e == current_index);
                        if (ImGui::Selectable(v.name, is_selected)) {
                            current_index = e;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                    *(int *)cont.data = type->values[current_index].value;
                }
            }
        }
    } else if (type->category == INTRO_ARRAY) {
        if (do_tree_place_holder) {
            if (intro_has_attribute_x(ctx, attr, GUIATTR(edit_text))) {
                ImGui::InputText("##", (char *)cont.data, type->count);
            } else {
                ImGui::TextDisabled("---");
            }
        }
        if (is_open) {
            edit_array(ctx, &cont, (length > 0)? length : type->count);
            ImGui::TreePop();
        }
    } else if (type->category == INTRO_POINTER) {
        void * ptr_data = *(void **)cont.data;
        if (intro_has_attribute_x(ctx, attr, ctx->attr.builtin.cstring)) {
            ImGui::Text("\"%s\"", (const char *)ptr_data);
        } else if (ptr_data) {
            ImGui::TextColored(ptr_color, "0x%llx", (uintptr_t)ptr_data);
            if (!has_length) length = 1;
            if (length > 0) {
                if (is_open) {
                    edit_array(ctx, &cont, length);
                }
            }
        } else {
            ImGui::TextDisabled("NULL");
        }
        if (is_open) {
            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("<unimplemented>");
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

} // namespace intro

void
intro_imgui_edit_x(IntroContext * ctx, IntroContainer cont, const char * name) {
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (ImGui::BeginTable(name, 3, flags)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("value");
        ImGui::TableHeadersRow();

        intro::edit_member(ctx, name, cont, (int)(uintptr_t)name);
        ImGui::EndTable();
    }
}
#undef GUIATTR
#endif // INTRO_IMGUI_IMPL

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INTRO_H
