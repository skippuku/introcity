#ifndef INTRO_H
#define INTRO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __INTRO__
#define I(...)
#endif

#define ITYPE(x) (&__intro_types[ITYPE_##x])

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
} IntroCategory;

typedef enum {
    INTRO_UNSIGNED = 0x10,
    INTRO_SIGNED   = 0x20,
    INTRO_FLOATING = 0x30,
} IntroCategoryFlags;

typedef struct IntroStruct IntroStruct;
typedef struct IntroEnum IntroEnum;
typedef struct IntroFunction IntroFunction;
typedef struct IntroType IntroType;

struct IntroType {
    const char * name;
    IntroType * parent;
    IntroCategory category;
    union {
        uint32_t array_size;
        IntroStruct * i_struct;
        IntroEnum * i_enum;
        IntroFunction * function;
    };
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
    uint32_t offset;
    uint32_t count_attributes;
    const IntroAttributeData * attributes;
} IntroMember;

struct IntroStruct {
    uint32_t size;
    uint32_t count_members;
    bool is_union;
    IntroMember members [];
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
    IntroEnumValue members [];
};

typedef struct IntroArgument {
    const char * name;
    IntroType * type;
} IntroArgument;

struct IntroFunction {
    IntroType * return_type;
    uint32_t count_arguments;
    IntroArgument arguments [];
};

typedef struct IntroContext {
    IntroType * types;
    const char ** notes;
    uint8_t * values;

    uint32_t count_types;
    uint32_t count_notes;
    uint32_t size_values;
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
         || type->category == INTRO_ENUM
         || type->category == INTRO_FUNCTION);
}

typedef struct {
    int indent;
} IntroPrintOptions;

int intro_size(const IntroType * type);
const IntroType * intro_base(const IntroType * type, int * o_depth);
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

#endif // INTRO_H
