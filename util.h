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

enum ErrorType {
    ERR_IRRELEVANT = -1,
    ERR_NONE = 0,
    ERR_FILE_NOT_FOUND = 1,
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

typedef struct {
    char * buffer;
    NameSet * ignore_typedefs;
    struct{char * key; IntroType * value;} * type_map;
    struct{IntroType key; IntroType * value;} * type_set;
    NameSet * name_set;
    NestInfo * nest_map;

    uint8_t * value_buffer;
    PtrStore * ptr_stores;

    DifferedDefault * differed_length_defaults;

    ExprContext * expr_ctx;
} ParseContext;

static IntroType * parse_base_type(ParseContext *, char **, Token *, bool);
static IntroType * parse_declaration(ParseContext *, IntroType *, char **, Token *);

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
            arrsetcap(*p_str, (prev_cap)? prev_cap << 1 : 64);
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
