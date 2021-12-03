#ifndef INTRO_H
#define INTRO_H
#include <stdint.h>
typedef struct IntroStruct IntroStruct;

// IMPORTANT(cy):
// Make sure this matches with the array in intro.c
typedef enum IntroCategory {
    INTRO_UNKNOWN = 0,

    INTRO_FLOATING,
    INTRO_SIGNED,
    INTRO_UNSIGNED,

    INTRO_STRUCT,
    INTRO_ENUM, // NOT IMPLEMENTED
    INTRO_ARRAY, // NOT IMPLEMENTED

    INTRO_TYPE_COUNT
} IntroCategory;

typedef struct IntroType {
    char * name;
    uint32_t size;
    IntroCategory category;
    uint16_t pointer_level;
    IntroStruct * i_struct;
} IntroType;

typedef struct IntroMember {
    char * name;
    IntroType * type;
    uint32_t offset;
} IntroMember;

struct IntroStruct {
    char * name;
    bool is_union; // NOT IMPLEMENTED
    uint32_t count_members;
    IntroMember members [];
};
#endif
