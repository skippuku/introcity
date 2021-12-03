#ifndef BASIH_H
#define BASIC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

#ifdef DEBUG
# define db_assert(x) assert(x)
#else
# define db_assert(x)
#endif

#endif
