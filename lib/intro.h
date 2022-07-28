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
    INTRO_CONST = 0x01,
    INTRO_STATIC = 0x02,
    INTRO_INLINE = 0x04,
    INTRO_EXPLICITLY_GENERATED = 0x08,
    INTRO_HAS_BODY = 0x10,
    INTRO_IS_FLAGS = 0x20,

    // reuse
    INTRO_IS_SEQUENTIAL = INTRO_HAS_BODY,
} IntroFlags;

typedef struct IntroType IntroType I(~gui_edit);

typedef struct IntroMember {
    const char * name;
    IntroType * type;
    uint32_t offset;
    uint32_t attr;
} IntroMember;

typedef struct IntroEnumValue {
    const char * name;
    int32_t value;
} IntroEnumValue;

struct IntroType {
    union {
        void * __data I(~gui_show);
        IntroType * of          I(when <-category == INTRO_ARRAY || <-category == INTRO_POINTER);
        IntroMember * members   I(/* length <-count, */ when (<-category & 0xf0) == INTRO_STRUCT);
        IntroEnumValue * values I(/* length <-count, */ when <-category == INTRO_ENUM);
        IntroType ** arg_types  I(/* length <-count, */ when <-category == INTRO_FUNCTION);
    } INTRO_TYPE_UNION_NAME;
    IntroType * parent;
    const char * name;
    uint32_t count;
    uint32_t attr;
    uint32_t size;
    uint16_t flags I(gui_format "0x%02x");
    uint8_t align;
    uint8_t category;
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
    INTRO_AT_STRING,
    INTRO_AT_TYPE, // unimplemented
    INTRO_AT_EXPR,
    INTRO_AT_REMOVE,
    INTRO_AT_COUNT
} IntroAttributeType;

typedef struct IntroAttribute {
    const char * name;
    int attr_type;
    uint32_t type_id;
} IntroAttribute;

typedef struct IntroAttributeSpec {
    INTRO_ALIGN(16) uint32_t bitset [(INTRO_MAX_ATTRIBUTES+31) / 32];
    //uint32_t value_offsets []; // Flexible array members are not officially part of C++
} IntroAttributeSpec;

typedef struct IntroBuiltinAttributeIds {
    uint8_t i_id;
    uint8_t i_btfld;
    uint8_t i_default;
    uint8_t i_length;
    uint8_t i_alias;
    uint8_t i_city;
    uint8_t i_cstring;
    uint8_t i_type;
    uint8_t i_when;
    uint8_t i_remove;

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
    IntroType * types     I(length count_types);
    const char ** strings I(length count_strings);
    uint8_t * values      I(length size_values);
    IntroFunction * functions I(length count_functions);
    IntroMacro * macros   I(length count_macros);

    uint32_t count_types;
    uint32_t count_strings;
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

I(attribute i_ (
    id:       int,
    btfld:    int,
    default:  value(@inherit),
    length:   member, // TODO: change to expr
    alias:    string,
    city:     flag @global,
    cstring:  flag,
    type:     flag,
    when:     expr,
    remove:   __remove,
))

I(attribute gui_ (
    note:   string,
    name:   string,
    min:    value(@inherit),
    max:    value(@inherit),
    format: string,
    scale:  float,
    vector: flag,
    color:  value(IntroGuiColor),
    show:   flag @global,
    edit:   flag @global,
    edit_color: flag,
    edit_text:  flag,
))

I(apply_to (char *) (cstring))
I(apply_to (void *) (~city))

#define intro_var_get(var, T) (assert(var.type == ITYPE(T)), *(T *)var.data)
#define intro_var_ptr(var, T) ((type->of == ITYPE(T))? (T *)var.data : (T *)0)

INTRO_API_INLINE bool
intro_is_scalar(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

INTRO_API_INLINE bool
intro_is_int(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_S64);
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

INTRO_API_INLINE int
intro_size(const IntroType * type) {
    return type->size;
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
intro_container(void * data, const IntroType * type) {
    IntroContainer container;
    container.data = (uint8_t *)data;
    container.type = type;
    container.parent = NULL;
    container.index = 0;
    return container;
}

typedef struct {
    int indent;
} IntroPrintOptions;

typedef struct IntroPool IntroPool;

// ATTRIBUTE INFO
#define intro_attribute_value(m, a, out) intro_attribute_value_x(INTRO_CTX, m->type, m->attr, IATTR_##a, out)
bool intro_attribute_value_x(IntroContext * ctx, const IntroType * type, uint32_t attr_spec, uint32_t attr_id, IntroVariant * o_var);
#define intro_attribute_int(m, a, out) intro_attribute_int_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_int_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, int32_t * o_int);
#define intro_attribute_member(m, a, out) intro_attribute_member_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_member_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, int32_t * o_int);
#define intro_attribute_float(m, a, out) intro_attribute_float_x(INTRO_CTX, m->attr, IATTR_##a, out)
bool intro_attribute_float_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id, float * o_float);
#define intro_attribute_string(m, a) intro_attribute_string_x(INTRO_CTX, m->attr, IATTR_##a)
const char * intro_attribute_string_x(IntroContext * ctx, uint32_t attr_spec, uint32_t attr_id);
#define intro_attribute_length(c, ct, m, out) intro_attribute_length_x(INTRO_CTX, c, ct, m, out)
bool intro_attribute_length_x(IntroContext * ctx, const void * container, const IntroType * container_type, const IntroMember * m, int64_t * o_length);

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
#define intro_imgui_edit(data, data_type) intro_imgui_edit_x(INTRO_CTX, data, data_type, #data)
void intro_imgui_edit_x(IntroContext * ctx, void * data, const IntroType * data_type, const char * name);

// MISC
IntroContainer intro_push(const IntroContainer * parent, int32_t index);
IntroType * intro_type_with_name_x(IntroContext * ctx, const char * name);
const char * intro_enum_name(const IntroType * type, int value);
int64_t intro_int_value(const void * data, const IntroType * type);
#define intro_member_by_name(t, name) intro_member_by_name_x(t, #name)
const IntroMember * intro_member_by_name_x(const IntroType * type, const char * name);

#ifdef __cplusplus
}
#endif
#endif // INTRO_H
