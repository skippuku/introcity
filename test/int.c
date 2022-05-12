#include "test.h"

void
check(int index, const char * str, IntroCategory a, IntroCategory b) {
    if (a != b) {
        fprintf(stderr, "m[%i].type%s->category: expected 0x%x, got 0x%x\n", index, str, (int)b, (int)a);
        exit(134);
    }
}

int
main() {
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

        CHECK0(14,,                  ARRAY);
        CHECK0(14, ->parent,         POINTER);
        CHECK0(14, ->parent->parent, U64);

        CHECK0(15,,                  ARRAY);
        CHECK0(15, ->parent,         POINTER);
        CHECK0(15, ->parent->parent, U64);

        CHECK0(16,,                  POINTER);
        CHECK0(16, ->parent,         ARRAY);
        CHECK0(16, ->parent->parent, U64);

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
