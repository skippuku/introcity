#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <intro.h>

typedef struct {
    char c0, c1;
} CharX2;

typedef struct {
    char ca, cb;
    int i;
    void * ptr;
    short shorty;
    char * str;
    CharX2 double_char;
} LayoutTest;

typedef union {
    double a;
    LayoutTest b;
    CharX2 c;
} UnionTest;

#include "layout.c.intro"

const IntroMember *
get_member(const IntroType * type, char * name) {
    for (int i=0; i < type->i_struct->count_members; i++) {
        const IntroMember * m = &type->i_struct->members[i];
        if (0==strcmp(m->name, name)) {
            return m;
        }
    }
    return NULL;
}

#define CHECK_SIZE(t) assert(ITYPE(t)->size == sizeof(t))
#define CHECK_ALIGN(t) assert(ITYPE(t)->align == _Alignof(t))
#define CHECK_OFFSET(t, name) assert(get_member(ITYPE(t), #name)->offset == offsetof(t, name))

int
main() {
    CHECK_SIZE(LayoutTest);
    CHECK_ALIGN(LayoutTest);
    CHECK_OFFSET(LayoutTest, ca);
    CHECK_OFFSET(LayoutTest, cb);
    CHECK_OFFSET(LayoutTest, i);
    CHECK_OFFSET(LayoutTest, ptr);
    CHECK_OFFSET(LayoutTest, shorty);
    CHECK_OFFSET(LayoutTest, str);
    CHECK_OFFSET(LayoutTest, double_char);

    CHECK_SIZE(UnionTest);
    CHECK_ALIGN(UnionTest);
    CHECK_OFFSET(UnionTest, a);
    CHECK_OFFSET(UnionTest, b);
    CHECK_OFFSET(UnionTest, c);

    CHECK_SIZE(IntroType);
    CHECK_OFFSET(IntroType, category);
    CHECK_OFFSET(IntroType, flags);
    CHECK_OFFSET(IntroType, __data);
    CHECK_OFFSET(IntroType, i_struct);
    CHECK_OFFSET(IntroType, i_enum);
    CHECK_OFFSET(IntroType, array_size);
    CHECK_OFFSET(IntroType, of);
    CHECK_OFFSET(IntroType, parent);
    CHECK_OFFSET(IntroType, name);
    CHECK_OFFSET(IntroType, attr);
    CHECK_OFFSET(IntroType, size);
    CHECK_OFFSET(IntroType, align);

    return 0;
}
