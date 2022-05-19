// SPECIAL MACRO TESTS
#define SOME_ERR(msg) fprintf(stderr, "error (%s:%i): %s\n", msg, __FILE__, __LINE__)
#define cat(a, b) a ## b
#define UNIQUE_ITER(start, end) UNIQUE_ITER_x(start, end, __COUNTER__)
#define UNIQUE_ITER_x(start, end, c) (int cat(_i_, c)=0; cat(_i_, c) < end; cat(_i_, c)++)
#define DISPLAY(iden) #iden ==> iden

int
special_macros() {
    DISPLAY(__FILE__);
    DISPLAY(__LINE__);
    DISPLAY(__DATE__);
    DISPLAY(__TIME__);
    DISPLAY(__BASE_FILE__);
    DISPLAY(__FILE_NAME__);
    DISPLAY(__TIMESTAMP__);
    DISPLAY(__INCLUDE_LEVEL__);

    SOME_ERR("oh no.");

    UNIQUE_ITER(0, 5);
    UNIQUE_ITER(1, 12); UNIQUE_ITER(2, 18);
}

// error highlighting test
#define BUILD_DYNAMIC_ARRAY(type) \
    struct { \
        type * data; \
        int len; \
        int cap; \
    }

typedef struct {float x,y;} RealType;

typedef BUILD_DYNAMIC_ARRAY(int) *int_arr_t;

#include "pre_inc.h"

typedef BUILD_DYNAMIC_ARRAY(NotAType) *test_err_arr_t; // intentional error

// from the c99 standard
int
std_examples() {
    // 6.10.3.3.4
    #define hash_hash # ## #
    #define mkstr(a) # a
    #define in_between(a) mkstr(a)
    #define join(c, d) in_between(c hash_hash d)
    char p[] = join(x, y);

    // 6.10.3.5.6
    #define str(s)      # s
    #define xstr(s)     str(s)
    #define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \

    #define INCFILE(n)  vers ## n
    #define glue(a, b)  a ## b
    #define xglue(a, b) glue(a, b)
    #define HIGHLOW     "hello"
    #define LOW         LOW ", world"
    debug(1, 2);
    fputs(str(strncmp("abc\0d", "abc", '\4') // this goes away
    == 0) str(: @\n), s);
    #include xstr(INCFILE(2).h)
    glue(HIGH, LOW);
    xglue(HIGH, LOW)

    #undef str
    // 6.10.3.5.5
    #define x 3
    #define f(a) f(x * (a))
    #undef x
    #define x 2
    #define g f
    #define z z[0]
    #define h g(~
    #define m(a) a(w)
    #define w 0,1
    #define t(a) a
    #define p() int
    #define q(x) x
    #define r(x,y) x ## y
    #define str(x) # x
    f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
    g(x+(3,4)-w) | h 5) & m
    (f)^m(m);
    p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };
    char c[2][6] = { str(hello), str() };
}
