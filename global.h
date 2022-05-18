#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lib/intro.h"
#include "lib/ext/stb_ds.h"
#include "lib/ext/stb_sprintf.h"

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))? (a) : (b))
#endif

#define strputnull(a) arrput(a,0)
// index of last put or get
#define shtemp(t) stbds_temp((t)-1)
#define hmtemp(t) stbds_temp((t)-1)

#define STACK_TERMINATE(name, src, length) \
    char name [length + 1]; \
    memcpy(name, src, length); \
    name[length] = 0;

#if defined __has_attribute
  #if __has_attribute(unused)
    #define UNUSED __attribute__((unused))
  #endif
#endif
#ifndef UNUSED
  #define UNUSED
#endif

#define HERE() fprintf(stderr, "here: %s:%i\n", __FILE__, __LINE__)

typedef struct {
    const char * key;
} NameSet;

typedef struct {
    void * key;
    int32_t value;
} IndexByPtrMap;

typedef struct {
    void * key;
    IntroType * parent;
    int member_index;
    int indirection_level;
    char * parent_member_name;
    const char * top_level_name;
} NestInfo;

typedef struct IntroInfo {
    uint32_t count_types;
    IntroType ** types;
    IndexByPtrMap * index_by_ptr_map;
    NestInfo * nest_map;
    uint8_t * value_buffer;
} IntroInfo;

enum ReturnCode {
    RET_IRRELEVANT_ERROR = -1,
    RET_OK = 0,

    RET_FILE_NOT_FOUND = 2,
    RET_NOT_DEFINITION = 3,
    RET_DECL_CONTINUE = 4,
    RET_DECL_FINISHED = 5,
    RET_FOUND_END = 6,
    RET_NOT_TYPE = 7,
    RET_DECL_VA_LIST = 8,
};

typedef struct {
    ptrdiff_t value_offset;
    void * data;
    size_t data_size;
} PtrStore;

typedef struct {
    int32_t member_index, attribute_type;
    uint32_t value;
} DifferedDefault;

typedef struct ExprContext ExprContext;

typedef enum {
    KEYW_INVALID = -1,

    KEYW_CONST = 0,
    KEYW_STATIC,
    KEYW_STRUCT,
    KEYW_UNION,
    KEYW_ENUM,
    KEYW_TYPEDEF,
    KEYW_VOLATILE,
    KEYW_INLINE,
    KEYW_RESTRICT,
    KEYW_EXTERN,

    KEYW_UNSIGNED,
    KEYW_SIGNED,
    KEYW_INT,
    KEYW_LONG,
    KEYW_CHAR,
    KEYW_SHORT,
    KEYW_FLOAT,
    KEYW_DOUBLE,
    KEYW_MS_INT32,
    KEYW_MS_INT64,

    KEYW_COUNT
} Keyword;

typedef struct {
    char * buffer;
    NameSet * ignore_typedefs;
    struct{char * key; IntroType * value;} * type_map;
    struct{IntroType key; IntroType * value;} * type_set;
    NameSet * keyword_set;
    NameSet * name_set;
    NestInfo * nest_map;

    uint8_t * value_buffer;
    PtrStore * ptr_stores;

    DifferedDefault * differed_length_defaults;

    ExprContext * expr_ctx;
} ParseContext;

typedef struct {
    IntroType * base;
    IntroType * type;
    Token base_tk;
    Token name_tk;

    enum {
        DECL_NORMAL = 0,
        DECL_TYPEDEF,
        DECL_CAST,
        DECL_ARGS,
    } state;
} DeclState;

static int parse_declaration(ParseContext * ctx, char ** o_s, DeclState * decl);

#if defined(__has_attribute)
  #if defined(__MINGW32__)
    #define STRPUTF_FORMAT __MINGW_PRINTF_FORMAT
  #else
    #define STRPUTF_FORMAT printf
  #endif
__attribute__ ((format (STRPUTF_FORMAT, 2, 3)))
#endif
static void
strputf(char ** p_str, const char * format, ...) {
    va_list args_original;
    va_start(args_original, format);

    while (1) {
        va_list args;
        va_copy(args, args_original);

        if (*p_str == NULL) arrsetcap(*p_str, 128);

        char * loc = *p_str + arrlen(*p_str);
        size_t n = arrcap(*p_str) - arrlen(*p_str);
        size_t pn;
        if (n > 0) {
            pn = stbsp_vsnprintf(loc, n, format, args);
        } else {
            // NOTE: this is here due to strange behavior of stbsp when n == 0 (the byte before loc is set to 0)
            // this might be a bug with stbsp
            pn = 1;
        }
        if (pn < n) {
            arrsetlen(*p_str, arrlen(*p_str) + pn);
            break;
        } else {
            size_t prev_cap = arrcap(*p_str);
            arrsetcap(*p_str, prev_cap << 1);
        }

        va_end(args);
    }

    va_end(args_original);
}

#ifdef DEBUG
// this is so i can get array and map length in gdb
int
dbarrlen(void * a) {
    return (a)? arrlen(a) : -1;
}

int
dbhmlen(void * m) {
    return (m)? hmlen(m) : -1;
}
#endif
#endif // UTIL_H
