#ifndef UTIL_C
#define UTIL_C

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
void
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
            arrsetcap(*p_str, p_cap ? (p_cap << 1) : 64);
        }

        va_end(args);
    }

    va_end(args_original);
}

#endif
