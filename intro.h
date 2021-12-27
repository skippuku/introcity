#ifndef INTRO_H
#define INTRO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

typedef struct IntroMember {
    char * name;
    IntroType * type;
    uint32_t offset;
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
