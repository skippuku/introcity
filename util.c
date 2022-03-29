#ifndef UTIL_C
#define UTIL_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOFLOAT
#include "stb_sprintf.h"

#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#define strputnull(a) arrput(a,0)
// index of last put or get
#define shtemp(t) stbds_temp((t)-1)
#define hmtemp(t) stbds_temp((t)-1)

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
        size_t pn = stbsp_vsnprintf(loc, n, format, args);
        if (pn < n) {
            arrsetlen(*p_str, arrlen(*p_str) + pn);
            break;
        } else {
            size_t p_cap = arrcap(*p_str);
            arrsetcap(*p_str, (p_cap)? p_cap << 1 : 64);
        }

        va_end(args);
    }

    va_end(args_original);
}

static size_t
fsize(FILE * file) {
    long location = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t result = ftell(file);
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
