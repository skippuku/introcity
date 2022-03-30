//#include "basic.h" // TODO: fix

typedef struct {
    char * name;
    int32_t a, b;
    uint64_t array [8];
} Basic;

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
    uint8_t buffer [4];
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
    int32_t id;
    struct {
        char * name;
        int32_t id;
        struct {
            int32_t age;
            enum {
                FRUIT_APPLE,
                FRUIT_BANANA,
                FRUIT_PEAR,
                FRUIT_PEACH,
                FRUIT_PLUM,
                FRUIT_ME,
                FRUIT_MAX,
            } favorite_fruit;
        } son;
    } son;
    union {
        int32_t id;
        float speed;
    } daughter;

    Skills skills;
} Nest;

typedef struct TestAttributes {
    uint8_t * buffer I(1, length buffer_size);
    int32_t buffer_size;
    uint32_t v1 I(3);
    int h I(2, note "notes test, hello");
} TestAttributes;

#if 0
typedef struct {
    char * name;
    Forward;
} MsExt;
#endif
