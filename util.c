#ifndef UTIL_C
#define UTIL_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lib/ext/stb_ds.h"
#include "lib/ext/stb_sprintf.h"

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#endif
#define strputnull(a) arrput(a,0)
// index of last put or get
#define shtemp(t) stbds_temp((t)-1)
#define hmtemp(t) stbds_temp((t)-1)

typedef struct {
    void * key;
    int32_t value;
} IndexByPtrMap;

typedef struct {
    void * key;
    IntroType * parent;
    int member_index;
    char * parent_member_name;
    char * top_level_name;
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

static long
fsize(FILE * file) {
    long location = ftell(file);
    fseek(file, 0, SEEK_END);
    long result = ftell(file);
    fseek(file, location, SEEK_SET);
    return result;
}

static char *
read_entire_file(const char * filename, size_t * o_size) {
    FILE * file = fopen(filename, "rb");
    if (!file) return NULL;
    size_t file_size = fsize(file);
    char * buffer = malloc(file_size + 1);
    if (fread(buffer, file_size, 1, file) != 1) {
        fclose(file);
        free(buffer);
        return NULL;
    }
    fclose(file);
    buffer[file_size] = '\0';
    if (o_size) *o_size = file_size;
    return buffer;
}

static int
dump_to_file(const char * filename, void * data, size_t data_size) {
    FILE * file = fopen(filename, "wb");
    if (!file) return -1;
    int res = fwrite(data, data_size, 1, file);
    fclose(file);
    return (res == 1)? 0 : -1;
}

#endif
