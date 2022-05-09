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

__attribute__ ((format (printf, 2, 3)))
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
// this is so i can get the array length in gdb
int
dbarrlen(void * a) {
    return (a)? arrlen(a) : -1;
}
#endif
#endif
