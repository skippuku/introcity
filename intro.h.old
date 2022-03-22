#ifndef INTRO_H
#define INTRO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define I(...)

static const uint32_t INTRO_ZERO_LENGTH = UINT32_MAX;
static const uint32_t INTRO_POINTER = 0;

typedef struct IntroStruct IntroStruct;
typedef struct IntroEnum IntroEnum;

// IMPORTANT:
// Make sure this matches with the array in intro.c
typedef enum IntroCategory {
    INTRO_UNKNOWN = 0,

    INTRO_FLOATING,
    INTRO_SIGNED,
    INTRO_UNSIGNED,

    INTRO_STRUCT,
    INTRO_ENUM,

    INTRO_CATEGORY_COUNT
} IntroCategory;

typedef struct IntroType {
    char * name;
    uint32_t size;
    uint16_t category;
    uint16_t indirection_level;
    union {
        IntroStruct * i_struct;
        IntroEnum * i_enum;
    };
    uint32_t * indirection;
} IntroType;

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
        // bool (*condition_func)(void * struct_ptr);
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
    char * name;
    uint32_t count_members;
    bool is_union;
    IntroMember members [];
};

typedef struct IntroEnumValue {
    int value;
    char * name;
} IntroEnumValue;

struct IntroEnum {
    char * name;
    uint32_t count_members;
    bool is_flags;
    bool is_sequential;
    IntroEnumValue members [];
};
#endif
