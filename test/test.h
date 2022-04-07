#include "basic.h"

typedef struct {
    char * name;
    int32_t a, b;
    uint8_t array [8];
} Basic;

typedef struct {
    char * name;
    int32_t a, b, c;
    uint8_t array [8];
    bool has_c;
    IntroType * type I(type);
} BasicPlus;

struct Undefined1;
typedef struct Undefined2 Undefined2;
enum UndefEnum;

typedef struct Forward Forward;

typedef struct {
    char c;
    short s;
    int i;
    long l;
    long long ll;

    signed char sc;
    signed int si;

    unsigned char uc;
    unsigned short int us;
    unsigned u;
    unsigned int ui;
    unsigned long long int v1, *v2, v3[3], *v4[4], *(v5[5]), (*v6)[6];
} TestInt;

struct TestPtr {
    Forward * p_forward;
    char **** indirect_4;
    struct TestPtr * recur;
};

struct Forward {
    u8 buffer [4];
    int i;
} inline_declaration;

typedef enum Skills {
    SKILL_PROGRAMMER  = 0x01,
    SKILL_WRITER      = 0x02,
    SKILL_ARTIST      = 0x04,
    SKILL_MUSICIAN    = 0x08,
    SKILL_ACROBAT     = 0x10,
    SKILL_BASEBALLBAT = 0x20,
} Skills;

typedef struct Nest {
    char * name;
    s32 id;
    struct {
        char * name;
        s32 id;
        struct {
            s32 age;
            enum {
                FRUIT_APPLE,
                FRUIT_BANANA,
                FRUIT_PEAR,
                FRUIT_PEACH,
                FRUIT_PLUM,
                FRUIT_ME,
                FRUIT_COUNT,
            } favorite_fruit;
        } son;
    } son;
    union {
        s32 id;
        float speed;
    } daughter;

    Skills skills;
} Nest;

typedef struct TestAttributes {
    u8 * buffer I(1, length buffer_size);
    s32 buffer_size;
    u32 v1 I(3);
    int h I(2, note "notes test, hello");
} TestAttributes;

enum {
    ANON_UNSEEN,
    ANON_INVISIBLE,
};

struct {
    int v1;

    struct {
        int v1;
    } nested;

    enum {
        GLOBAL_MAIN,
        GLOBAL_PAUSE,
        GLOBAL_LOADING,
    } state;
} global_state;

typedef struct {
    struct Undefined1 * s_u1;
    Undefined2 * u2;
    struct Undefined2 * s_u2;

    enum UndefEnum * e_;

    //Undefined2 fail_0;
    //struct Undefined1 fail_1;
    //struct Undefined2 fail_2;
    //enum UndefEnum faile_enum;
} TestUndefined;

#if 0
typedef struct {
    char * name;
    Forward;
} MsExt;
#endif
