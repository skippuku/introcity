#ifndef INTRO_H
#define INTRO_H

#include <stdint.h>
#include <stdbool.h>

#ifndef __INTROCITY__
#define I(...)
#endif

typedef enum IntroCategory {
    INTRO_UNKNOWN = 0x0,

    INTRO_U8  = 0x011,
    INTRO_U16 = 0x012,
    INTRO_U32 = 0x014,
    INTRO_U64 = 0x018,

    INTRO_S8  = 0x021,
    INTRO_S16 = 0x022,
    INTRO_S32 = 0x024,
    INTRO_S64 = 0x028,

    INTRO_F32 = 0x044,
    INTRO_F64 = 0x048,

    INTRO_ARRAY   = 0x100,
    INTRO_POINTER = 0x200,

    INTRO_STRUCT  = 0x400,
    INTRO_UNION   = 0x410,
    INTRO_ENUM    = 0x800,
} IntroCategory;

typedef enum {
    INTRO_UNSIGNED = 0x010,
    INTRO_SIGNED   = 0x020,
    INTRO_FLOATING = 0x040,
} IntroCategoryFlags;

typedef struct IntroStruct IntroStruct;
typedef struct IntroEnum IntroEnum;
typedef struct IntroType IntroType;

struct IntroType {
    char * name;
    IntroType * parent;
    IntroCategory category;
    union {
        uint32_t array_size;
        IntroStruct * i_struct;
        IntroEnum * i_enum;
    };
};

typedef enum IntroAttribute {
    INTRO_ATTR_ID      = -16,
    INTRO_ATTR_DEFAULT = -15,
    INTRO_ATTR_LENGTH  = -14,
    INTRO_ATTR_SWITCH  = -13,
    INTRO_ATTR_TYPE    = -12,
    INTRO_ATTR_NOTE    = -11,
} IntroAttribute;

typedef struct IntroAttributeData {
    int32_t type;
    enum {
        INTRO_V_NONE,
        INTRO_V_INT,
        INTRO_V_FLOAT,
        INTRO_V_VALUE,
        INTRO_V_CONDITION,
        INTRO_V_MEMBER,
        INTRO_V_STRING,
    } value_type;
    union {
        int32_t i;
        float f;
    } v;
} IntroAttributeData;

typedef struct IntroMember {
    char * name;
    IntroType * type;
    uint32_t offset;
    uint32_t count_attributes;
    const IntroAttributeData * attributes;
} IntroMember;

struct IntroStruct {
    uint32_t count_members;
    bool is_union;
    IntroMember members [];
};

typedef struct IntroEnumValue {
    char * name;
    int32_t value;
} IntroEnumValue;

struct IntroEnum {
    uint32_t count_members;
    bool is_flags;
    bool is_sequential;
    IntroEnumValue members [];
};

typedef struct {
    void * key;
    int32_t value;
} IndexByPtrMap;

typedef struct IntroInfo {
    uint32_t count_types;
    IntroType ** types;
    IndexByPtrMap * index_by_ptr_map;
} IntroInfo;

#endif // INTRO_H
