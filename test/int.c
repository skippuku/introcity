#include "basic.h"
#include <intro.h>

typedef struct {
    char c; // 0
    short s;
    int i;
    long l;
    long long ll;

    signed char sc; // 5
    signed int si;

    unsigned char uc; // 7
    unsigned short int us;
    unsigned u;
    unsigned int ui;

    //                     11  12   13      14      15        16
    unsigned long long int v1, *v2, v3[3], *v4[4], *(v5[5]), (*v6)[6];

    unsigned long int uli; // 17
    signed long int sli;
    signed long long int slli;

    long unsigned int lui; // 20
    short unsigned su;
    long long signed int llsi;
    long signed ls;
    signed _s;
    signed int _si;
} TestInt;

#ifndef __INTRO__
#include "int.c.intro"
#endif

void
check(int index, const char * str, IntroCategory a, IntroCategory b) {
    if (a != b) {
        fprintf(stderr, "m[%i].type%s->category: expected 0x%x, got 0x%x\n", index, str, (int)b, (int)a);
        exit(134);
    }
}

int
main() {
    TestInt t;
    (void)t;
    const IntroType * test_int_type = ITYPE(TestInt);
    
    const IntroMember * m = test_int_type->i_struct->members;

    // TODO: check type parent ex. unsigned char -> uint8_t

#define CHECK0(i, p, x) check(i, #p, m[i].type p->category, INTRO_##x)
    CHECK0(0,,  S8);
    CHECK0(1,,  S16);
    CHECK0(2,,  S32);
    CHECK0(3,,  S64);
    CHECK0(4,,  S64);

    CHECK0(5,,  S8);
    CHECK0(6,,  S32);

    CHECK0(7,,  U8);
    CHECK0(8,,  U16);
    CHECK0(9,,  U32);
    CHECK0(10,, U32);

        CHECK0(11,, U64);

        CHECK0(12,,          POINTER);
        CHECK0(12, ->parent, U64);

        CHECK0(13,,          ARRAY);
        CHECK0(13, ->parent, U64);
        assert(m[13].type->array_size == LENGTH(t.v3));

        CHECK0(14,,                  ARRAY);
        CHECK0(14, ->parent,         POINTER);
        CHECK0(14, ->parent->parent, U64);
        assert(m[14].type->array_size == LENGTH(t.v4));

        CHECK0(15,,                  ARRAY);
        CHECK0(15, ->parent,         POINTER);
        CHECK0(15, ->parent->parent, U64);
        assert(m[15].type->array_size == LENGTH(t.v5));

        CHECK0(16,,                  POINTER);
        CHECK0(16, ->parent,         ARRAY);
        CHECK0(16, ->parent->parent, U64);
        assert(m[16].type->parent->array_size == LENGTH(*t.v6));

    CHECK0(17,, U64);
    CHECK0(18,, S64);
    CHECK0(19,, S64);

    CHECK0(20,, U64);
    CHECK0(21,, U16);
    CHECK0(22,, S64);
    CHECK0(23,, S64);
    CHECK0(24,, S32);
    CHECK0(25,, S32);

    return 0;
}
