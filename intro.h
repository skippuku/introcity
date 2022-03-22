#ifndef INTRO_H
#define INTRO_H

#include <stdint.h>

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
        IntroStruct * i_struct;
        IntroEnum * i_enum;
        uint32_t array_size;
    };
};

typedef struct IntroMember {
    char * name;
    IntroType * type;
    uint32_t offset;
} IntroMember;

struct IntroStruct {
    char * name;
    uint32_t count;
    bool is_union;
    IntroMember members [];
};

typedef struct IntroEnumValue {
    char * name;
    int32_t value;
} IntroEnumValue;

struct IntroEnum {
    char * name;
    uint32_t count;
    bool is_flags;
    bool is_sequential;
    IntroEnumValue members [];
};

typedef struct IntroInfo {
    uint32_t count_types;
    IntroType * types;
} IntroInfo;

#endif
