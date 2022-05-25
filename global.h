#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "lib/intro.h"
#include "lib/ext/stb_ds.h"
#include "lib/ext/stb_sprintf.h"

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(*(a)))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))? (a) : (b))
#endif

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

#define db_here() fprintf(stderr, "here: %s:%i\n", __FILE__, __LINE__)

#ifdef DEBUG
  #if (defined __GNUC__ || defined __clang__) && (defined __x86_64__ || defined __i386__)
    #define db_break() do{__asm__ __volatile__ ("int $3\n\t");}while(0)
  #elif defined __MSVC__
    #define db_break() __debugbreak()
  #endif
  #define db_assert(x) assert(x)
#else
  #define db_assert(x)
#endif
#ifndef db_break
  #define db_break()
#endif

#if defined __clang__
  #define COMPILER_STR "clang"
#elif defined __GNUC__
  #define COMPILER_STR "gcc"
#elif defined _MSC_VER
  #define COMPILER_STR "msvc"
#else
  #define COMPILER_STR "compiler"
#endif

typedef struct {
    int current;
    int current_used;
    int capacity;
    struct {
        void * data;
    } buckets [256]; // should be enough for anyone
} MemArena;

typedef struct {
    char * filename;
    char * buffer;
    size_t buffer_size;
    time_t mtime;
    bool once;
    bool gen; // whether type info should be generated for types declared in this file
} FileInfo;

typedef enum {
    LOC_NONE = 0,
    LOC_FILE,
    LOC_MACRO,
    LOC_POP,
} LocationEnum;

typedef struct {
    size_t offset;
    size_t file_offset;
    FileInfo * file;
    char * macro_name;
    LocationEnum mode;
} FileLoc;

typedef struct {
    FileInfo ** file_buffers;
    FileLoc * list;
    int64_t count;
    int64_t index;
    FileInfo * file;
    int * stack;
} LocationContext;

typedef struct {
    char * result_buffer;
    char * output_filename;
    LocationContext loc;
    int ret;
} PreInfo;

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
    IntroType ** types;
    IndexByPtrMap * index_by_ptr_map;
    NestInfo * nest_map;
    uint8_t * value_buffer;
    IntroTypePtrList ** arg_lists;
    IntroFunction ** functions;
    uint32_t count_types;
    uint32_t count_arg_lists;
    uint32_t count_functions;
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

typedef struct ParseContext ParseContext;

typedef struct {
    char * location;
    int32_t i;
    Token tk;
} AttributeSpecifier;

typedef struct {
    IntroType * base;
    IntroType * type;
    bool reuse_base;
    enum {
        DECL_GLOBAL = 1,
        DECL_TYPEDEF,
        DECL_CAST,
        DECL_ARGS,
        DECL_MEMBERS,
    } state;
    Token base_tk;
    Token name_tk;

    union {
        AttributeSpecifier * attribute_specifiers;
        char ** arg_names;
    };
    int32_t member_index;
    uint8_t bitfield;
} DeclState;

typedef struct {
    uint8_t size_ptr;
    uint8_t size_short;
    uint8_t size_int;
    uint8_t size_long;
    uint8_t size_long_long;
    uint8_t size_long_double;
    uint8_t size_bool;
    uint8_t char_is_signed;
} CTypeInfo;

typedef struct {
    char ** sys_include_paths;
    char * defines;
    MemArena * arena;
    CTypeInfo type_info;
} Config;

static int parse_declaration(ParseContext * ctx, char ** o_s, DeclState * decl);

static char *
strput_callback(const char * buf, void * user, int len) {
    char ** pstr = (char **)user;

    int str_len = arrlen(*pstr) + len;
    arrsetlen(*pstr, str_len);
    int prev_cap = arrcap(*pstr);

    if (prev_cap - str_len < STB_SPRINTF_MIN + 1) {
        arrsetcap(*pstr, str_len + STB_SPRINTF_MIN + 1);
    }
    char * out = *pstr + str_len;

    return out;
}

#if defined(__has_attribute)
  #if defined(__MINGW32__)
    #define STRPUTF_FORMAT __MINGW_PRINTF_FORMAT
  #else
    #define STRPUTF_FORMAT printf
  #endif
__attribute__ ((format (STRPUTF_FORMAT, 2, 3)))
#endif
static void
strputf(char ** pstr, const char * format, ...) {
    va_list args;
    va_start(args, format);

    stbsp_vsprintfcb(strput_callback, pstr, strput_callback(NULL, pstr, 0), format, args);
    (*pstr)[arrlen(*pstr)] = '\0'; // terminator does not add to length, so it is overwritten by subsequent calls

    va_end(args);
}

static void *
arena_alloc(MemArena * arena, size_t amount) {
    if (arena->current_used + amount > arena->capacity) {
        if (arena->buckets[++arena->current].data == NULL) {
            arena->buckets[arena->current].data = calloc(1, arena->capacity);
        }
        arena->current_used = 0;
    }
    void * result = arena->buckets[arena->current].data + arena->current_used;
    arena->current_used += amount;
    arena->current_used += 16 - (arena->current_used & 15);
    return result;
}

static MemArena *
new_arena(int capacity) {
    MemArena * arena = calloc(1, sizeof(MemArena));
    arena->capacity = capacity;
    arena->buckets[0].data = calloc(1, arena->capacity);
    return arena;
}

static void
reset_arena(MemArena * arena) {
    for (int i=0; i <= arena->current; i++) {
        memset(arena->buckets[i].data, 0, arena->capacity);
    }
    arena->current = 0;
    arena->current_used = 0;
}

static void
free_arena(MemArena * arena) {
    for (int i=0; i < LENGTH(arena->buckets); i++) {
        if (arena->buckets[i].data) free(arena->buckets[i].data);
    }
    free(arena);
}

static char *
copy_and_terminate(MemArena * arena, const char * str, int length) {
    char * result = arena_alloc(arena, length + 1);
    memcpy(result, str, length);
    result[length] = '\0';
    return result;
}

static char *
read_stream(FILE * file) {
    char * result = NULL;
    fseek(file, 0, SEEK_SET);
    const int read_size = 1024;
    char buf [read_size];
    while (1) {
        int read_res = fread(&buf, 1, read_size, file);
        memcpy(arraddnptr(result, read_res), buf, read_res);
        if (read_res < read_size) {
            arrput(result, '\0');
            break;
        }
    }
    return result;
}

static void
path_normalize(char * dest) {
    char * dest_start = dest;
    char * src = dest;
    while (*src) {
        if (*src == '\\') *src = '/';
        src++;
    }
    int depth = 0;
    src = dest;
    bool check_next = true;
    if (*src == '/') src++, dest++;
    char * last_dir = dest;
    while (*src) {
        if (check_next) {
            check_next = false;
            while (1) {
                if (memcmp(src, "/", 1)==0) {
                    src += 1;
                } else if (memcmp(src, "./", 2)==0) {
                    src += 2;
                } else if (memcmp(src, "../", 3)==0) {
                    if (depth > 0) {
                        dest = last_dir;
                        last_dir--;
                        while (--last_dir > dest_start && *last_dir != '/');
                        last_dir++;
                        depth -= 1;
                    } else {
                        depth = 0;
                        memmove(dest, src, 3);
                        dest += 3;
                    }
                    src += 3;
                } else {
                    last_dir = dest;
                    depth += 1;
                    break;
                }
            }
        }
        if (*src == '/') {
            check_next = true;
        }
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void
path_join(char * dest, const char * base, const char * ext) {
    strcpy(dest, base);
    strcat(dest, "/");
    strcat(dest, ext);
    path_normalize(dest);
}

static void
path_dir(char * dest, char * filepath, char ** o_filename) {
    char * end = strrchr(filepath, '/');
    if (end == NULL) {
        strcpy(dest, ".");
        if (o_filename) *o_filename = filepath;
    } else {
        size_t dir_length = end - filepath;
        memcpy(dest, filepath, dir_length);
        dest[dir_length] = '\0';
        if (o_filename) *o_filename = end + 1;
    }
}

static char *
path_extension(char * dest, const char * path) {
    char * forslash = strrchr(path, '/');
    char * period = strrchr(path, '.');
    if (!period || forslash > period) return NULL;

    return strcpy(dest, period);
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
