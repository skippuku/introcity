#ifndef INTRO_H
#define INTRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __INTRO__
#define I(...)
#endif

#define ITYPE(x) (&__intro_types[ITYPE_##x])

#ifndef INTRO_ANON_UNION_NAME
  #if __STDC_VERSION__ < 199901L
    #define INTRO_ANON_UNION_NAME u
  #else
    #define INTRO_ANON_UNION_NAME
  #endif
#endif

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
    int32_t line;
    int32_t column;
} IntroLocation;

typedef enum IntroFlags {
    INTRO_NONE = 0,
    INTRO_CONST = 0x01,
    INTRO_STATIC = 0x02,
    INTRO_INLINE = 0x04,
    INTRO_EXPLICITLY_GENERATED = 0x08,
} IntroFlags;

typedef struct IntroType IntroType;
typedef struct IntroStruct IntroStruct;
typedef struct IntroEnum IntroEnum;
typedef struct IntroTypePtrList IntroTypePtrList;

struct IntroType {
    const char * name;
    IntroType * parent;
    IntroCategory category;
    uint32_t flags; // not currently implemented
    union {
        void * __data;
        uint32_t array_size;
        IntroStruct * i_struct;
        IntroEnum * i_enum;
        IntroTypePtrList * args;
    } INTRO_ANON_UNION_NAME;
    IntroLocation location;
};

typedef enum IntroAttribute {
    INTRO_ATTR_ID      = -16,
    INTRO_ATTR_DEFAULT = -15,
    INTRO_ATTR_LENGTH  = -14,
    INTRO_ATTR_TYPE    = -13,
    INTRO_ATTR_NOTE    = -12,
    INTRO_ATTR_ALIAS   = -11,
} IntroAttribute;

typedef enum IntroAttributeValueType {
    INTRO_V_FLAG,
    INTRO_V_INT,
    INTRO_V_FLOAT,
    INTRO_V_VALUE,
    INTRO_V_MEMBER,
    INTRO_V_STRING,
} IntroAttributeValueType;

typedef struct IntroAttributeData {
    int32_t type;
    IntroAttributeValueType value_type;
    union {
        int32_t i;
        float f;
    } v;
} IntroAttributeData;

typedef struct IntroMember {
    const char * name;
    IntroType * type;
    uint8_t  bitfield;
    uint16_t id;
    uint32_t offset;
    uint32_t count_attributes;
    const IntroAttributeData * attributes I(length count_attributes);
} IntroMember;

struct IntroStruct {
    uint32_t size;
    uint32_t count_members;
    bool is_union;
    IntroMember members [] I(length count_members);
};

typedef struct IntroEnumValue {
    const char * name;
    int32_t value;
} IntroEnumValue;

struct IntroEnum {
    uint32_t size;
    uint32_t count_members;
    bool is_flags;
    bool is_sequential;
    IntroEnumValue members [] I(length count_members);
};

struct IntroTypePtrList {
    uint32_t count;
    IntroType * types [] I(length count);
};

typedef struct IntroFunction {
    const char * name;
    IntroType * type;
    IntroLocation location;
    uint32_t flags;
    bool has_body;
    const char * arg_names [];
} IntroFunction;

typedef struct IntroContext {
    IntroType * types          I(length count_types);
    const char ** notes        I(length count_notes);
    uint8_t * values           I(length size_values);
    IntroFunction ** functions I(length count_functions);

    uint32_t count_types;
    uint32_t count_notes;
    uint32_t size_values;
    uint32_t count_functions;
} IntroContext;

#ifndef INTRO_CTX
#define INTRO_CTX &__intro_ctx
#endif

#ifndef INTRO_INLINE
#define INTRO_INLINE static inline
#endif

#define intro_set_defaults(dest, type) intro_set_defaults_ctx(INTRO_CTX, dest, type)
#define intro_set_values(dest, type, attribute) intro_set_values_ctx(INTRO_CTX, dest, type, attribute)
#define intro_type_with_name(name) intro_type_with_name_ctx(INTRO_CTX, name)
#define intro_print(data, type, opt) intro_print_ctx(INTRO_CTX, data, type, opt)

INTRO_INLINE bool
intro_is_scalar(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

INTRO_INLINE bool
intro_is_int(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_S64);
}

INTRO_INLINE bool
intro_is_complex(const IntroType * type) {
    return (type->category == INTRO_STRUCT
         || type->category == INTRO_UNION
         || type->category == INTRO_ENUM);
}

INTRO_INLINE int
intro_size(const IntroType * type) {
    switch(type->category) {
    case INTRO_U8:
    case INTRO_S8:      return 1;
    case INTRO_U16:
    case INTRO_S16:     return 2;
    case INTRO_U32:
    case INTRO_S32:
    case INTRO_F32:     return 4;
    case INTRO_U64:
    case INTRO_S64:
    case INTRO_F64:     return 8;
    case INTRO_F128:    return 16;
    case INTRO_POINTER: return sizeof(void *);
    case INTRO_ARRAY:   return type->array_size * intro_size(type->parent);
    case INTRO_UNION:
    case INTRO_STRUCT:  return type->i_struct->size;
    case INTRO_ENUM:    return type->i_enum->size;
    default: return 0;
    }
}

INTRO_INLINE const IntroType *
intro_origin(const IntroType * type) {
    while (type->parent && type->category != INTRO_ARRAY && type->category != INTRO_POINTER) {
        type = type->parent;
    }
    return type;
}

typedef struct {
    int indent;
} IntroPrintOptions;

const char * intro_enum_name(const IntroType * type, int value);
int64_t intro_int_value(const void * data, const IntroType * type);
bool intro_attribute_flag(const IntroMember * m, int32_t attr_type);
bool intro_attribute_int(const IntroMember * m, int32_t attr_type, int32_t * o_int);
bool intro_attribute_float(const IntroMember * m, int32_t attr_type, float * o_float);
bool intro_attribute_length(const void * struct_data, const IntroType * struct_type, const IntroMember * m, int64_t * o_length);
void intro_set_member_value_ctx(IntroContext * ctx, void * dest, const IntroType * struct_type, int member_index, int value_attribute);
void intro_set_values_ctx(IntroContext * ctx, void * dest, const IntroType * type, int value_attribute);
void intro_set_defaults_ctx(IntroContext * ctx, void * dest, const IntroType * type);
void intro_sprint_type_name(char * dest, const IntroType * type);
void intro_print_type_name(const IntroType * type);
void intro_print_ctx(IntroContext * ctx, const void * data, const IntroType * type, const IntroPrintOptions * opt);
IntroType * intro_type_with_name_ctx(IntroContext * ctx, const char * name);

#define intro_load_city(dest, dest_type, data, data_size) intro_load_city_ctx(INTRO_CTX, dest, dest_type, data, data_size)
#define intro_load_city_file(dest, dest_type, filename) intro_load_city_file_ctx(INTRO_CTX, dest, dest_type, filename)

char * intro_read_file(const char * filename, size_t * o_size);
int intro_dump_file(const char * filename, void * data, size_t data_size);
void * intro_load_city_file_ctx(IntroContext * ctx, void * dest, const IntroType * dest_type, const char * filename);
bool intro_create_city_file(const char * filename, void * src, const IntroType * src_type);
void * intro_create_city(const void * src, const IntroType * s_type, size_t *o_size);
int intro_load_city_ctx(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, size_t data_size);

#define intro_imgui_edit(data, data_type, name) intro_imgui_edit_ctx(INTRO_CTX, data, data_type, name)
void intro_imgui_edit_ctx(IntroContext * ctx, void * data, const IntroType * data_type, const char * name);

#ifdef __cplusplus
}
#endif
#endif // INTRO_H
