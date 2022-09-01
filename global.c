#ifndef GLOBAL_C
#define GLOBAL_C

static uint64_t g_timer_freq = 0;
static DynAllocator * g_dynalloc = NULL;

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windef.h>
  #include <wingdi.h>
  #include <winbase.h>
  #include <wincon.h>
  #include <shlobj.h>
  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <sys/unistd.h>
#include <sys/stat.h>

#include "lib/intro.h"

//#define STBDS_REALLOC(CTX, PTR, SIZE) dyn_allocator_realloc(g_dynalloc, PTR, SIZE)
//#define STBDS_FREE(CTX, PTR) dyn_allocator_free(g_dynalloc, PTR)
#define STB_DS_IMPLEMENTATION
#include "ext/stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "ext/stb_sprintf.h"

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof*(a))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))? (a) : (b))
#endif

// index of last put or get
#define shtemp(t) stbds_temp((t)-1)
#define hmtemp(t) stbds_temp((t)-1)

#define STACK_TERMINATE(name, src, length) \
    char name [length + 1]; \
    memcpy(name, src, length); \
    name[length] = 0;

#define TERMINATE(DEST, SRC, LEN) do{ \
    assert(sizeof(DEST) > LEN); \
    memcpy(DEST, SRC, LEN); \
    (DEST)[LEN] = 0; \
}while(0)

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
  #elif defined _MSC_VER
    #define db_break() __debugbreak()
  #endif
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
#elif defined __TINYC__
  #define COMPILER_STR "tcc"
#else
  #define COMPILER_STR "compiler"
#endif

#include "lexer.c"

// REQUIREMENTS FOR EXPECT MACROS:
//   - ctx (ParseContext *), tk (Token), and tidx (TokenIndex *) are available
//   - function returns a return code (int)
#define EXPECT(x) \
    tk = next_token(tidx); \
    if (tk.start[0] != x) { \
        parse_error(ctx, tk, "Expected " #x "."); \
        return -1; \
    }

#define EXPECT_IDEN() \
    tk = next_token(tidx); \
    if (tk.type != TK_IDENTIFIER) { \
        parse_error(ctx, tk, "Expected identifier."); \
        return -1; \
    }

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
    char * string;
    enum {
        PRE_OP_INPUT_FILE,
        PRE_OP_DEFINE,
        PRE_OP_UNDEFINE,
    } type;
} PreOption;

typedef struct {
    char ** sys_include_paths;
    char * defines;
    MemArena * arena;
    CTypeInfo type_info;

    const char * program_name;
    const char * config_filename;
    const char * first_input_filename;

    char * output_filename;

    PreOption * pre_options;
    const char ** include_paths;
    const char * pragma;
    struct {
        char * custom_target;
        char * filename;
        enum {
            MT_NORMAL = 0,
            MT_SPACE,
            MT_NEWLINE,
        } target_mode;
        bool enabled : 1;
        bool D : 1;
        bool G : 1;
        bool P : 1;
        bool no_sys : 1;
        bool use_msys_path : 1;
    } m_options;

    bool gen_city : 1;
    bool gen_vim_syntax : 1;
    bool gen_typedefs : 1;
    bool show_metrics : 1;
    bool pre_only : 1;
} Config;

#define DEF_BUILTIN(NAME) {#NAME, offsetof(struct IntroBuiltinAttributes, NAME)}
static const struct {const char * key; int value;} g_builtin_attributes [] = {
    DEF_BUILTIN(id),
    DEF_BUILTIN(bitfield),
    DEF_BUILTIN(fallback),
    DEF_BUILTIN(length),
    DEF_BUILTIN(when),
    DEF_BUILTIN(alias),
    DEF_BUILTIN(imitate),
    DEF_BUILTIN(header),
    DEF_BUILTIN(city),
    DEF_BUILTIN(cstring),
    DEF_BUILTIN(remove),

    DEF_BUILTIN(gui_note),
    DEF_BUILTIN(gui_name),
    DEF_BUILTIN(gui_min),
    DEF_BUILTIN(gui_max),
    DEF_BUILTIN(gui_format),
    DEF_BUILTIN(gui_scale),
    DEF_BUILTIN(gui_vector),
    DEF_BUILTIN(gui_color),
    DEF_BUILTIN(gui_show),
    DEF_BUILTIN(gui_edit),
    DEF_BUILTIN(gui_edit_color),
    DEF_BUILTIN(gui_edit_text),
};
#undef DEF_BUILTIN

typedef struct {
    char * filename;
    Token * tk_list;
    char * buffer;
    size_t buffer_size;
    time_t mtime;
    bool once;
} FileInfo;

typedef enum {
    LOC_NONE = 0,
    LOC_FILE,
    LOC_MACRO,
    LOC_POP,
} LocationEnum;

typedef enum {
    NOTICE_NONE = 0,
    NOTICE_ENABLED = 0x01,
    NOTICE_FUNCTIONS = 0x02,
    NOTICE_MACROS = 0x04,

    NOTICE_INCLUDES = 0x0100,
    NOTICE_SYS_HEADERS = 0x0200,

    NOTICE_DEFAULT = NOTICE_ENABLED | NOTICE_INCLUDES,

    NOTICE_ALL = 0x00FF,
} NoticeState;

typedef struct {
    size_t offset;
    size_t file_offset;
    FileInfo * file;
    char * macro_name;
    LocationEnum mode;
    NoticeState notice;
} FileLoc;

typedef struct {
    FileInfo ** file_buffers;
    FileLoc * list;
    Token * tk_list;
    int64_t count;
    int64_t index;
    FileInfo * file;
    int * stack;
    NoticeState notice;
} LocationContext;

void
reset_location_context(LocationContext * lctx) {
    lctx->index = 0;
    arrsetlen(lctx->stack, 0);
}

typedef struct {
    Token * result_list;
    IntroMacro * macros;
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

typedef struct ParseInfo {
    IntroType ** types;
    IndexByPtrMap * index_by_ptr_map;
    uint8_t * value_buffer;
    IntroFunction ** functions;
    struct IntroAttributeContext attr;
    uint32_t count_types;
    uint32_t count_functions;
} ParseInfo;

enum ReturnCode {
    RET_FAILED_FILE_WRITE = -2,
    RET_IRRELEVANT_ERROR = -1,
    RET_OK = 0,

    RET_FILE_NOT_FOUND = 2,
    RET_NOT_DEFINITION = 3,
    RET_DECL_CONTINUE = 4,
    RET_DECL_FINISHED = 5,
    RET_FOUND_END = 6,
    RET_NOT_TYPE = 7,
    RET_DECL_VA_LIST = 8,
    RET_ALREADY_INCLUDED = 9,
};

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
    IntroType * base;
    IntroType * type;
    enum {
        DECL_GLOBAL = 1,
        DECL_TYPEDEF,
        DECL_CAST,
        DECL_ARGS,
        DECL_MEMBERS,
    } state;
    Token base_tk;
    Token name_tk;

    char ** arg_names;

    int32_t member_index;
    bool func_specifies_args;
    bool reuse_base;
} DeclState;

IntroType * parse_get_known(ParseContext * ctx, int index);
static int parse_declaration(ParseContext * ctx, TokenIndex * tidx, DeclState * decl);

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
path_dir(char * dest, char *restrict filepath, char ** o_filename) {
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

static struct Metrics {
    uint64_t start;
    uint64_t last;

    uint64_t file_access_time;
    uint64_t pre_time;
    uint64_t lex_time;

    uint64_t parse_time;
    uint64_t attribute_time;

    uint64_t gen_time;

    uint64_t count_pre_files;
    uint64_t count_pre_lines;
    uint64_t count_pre_tokens;
    uint64_t count_pre_file_bytes;
    uint64_t count_macro_expansions;

    uint64_t count_parse_tokens;
    uint64_t count_parse_types;
    uint64_t count_gen_types;
} g_metrics = {0};

static uint64_t
nanotime() {
#if _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
#elif defined _POSIX_TIMERS
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#else
    #pragma message ("No nanotime implementation for this platform")
    return 0;
#endif
}

static uint64_t
nanointerval() {
    uint64_t now = nanotime();
    uint64_t result = now - g_metrics.last;
    g_metrics.last = now;
    return result;
}

static void
show_metrics() {
    char * buf = NULL;
    strputf(&buf, "intro version %s", VERSION);
#ifdef DEBUG
    strputf(&buf, " (debug)");
#endif
    strputf(&buf, "\n");

#define AS_MSECS(t) ((t) / (double)g_timer_freq * 1000)
    uint64_t now = nanotime();
    strputf(&buf, "Total time: %.2fms\n", AS_MSECS(now - g_metrics.start));
    strputf(&buf, "|-Pre: %.2fms\n", AS_MSECS(g_metrics.pre_time + g_metrics.lex_time + g_metrics.file_access_time));
    strputf(&buf, "| |-File Access:  %.2fms\n", AS_MSECS(g_metrics.file_access_time));
    strputf(&buf, "| |-Tokenization: %.2fms\n", AS_MSECS(g_metrics.lex_time));
    strputf(&buf, "| |-Other:        %.2fms\n", AS_MSECS(g_metrics.pre_time));
    strputf(&buf, "|   %'11lu files\n", (unsigned long)g_metrics.count_pre_files);
    strputf(&buf, "|   %'11lu lines\n", (unsigned long)g_metrics.count_pre_lines);
    strputf(&buf, "|   %'11lu tokens\n", (unsigned long)g_metrics.count_pre_tokens);
    strputf(&buf, "|   %'11lu bytes\n", (unsigned long)g_metrics.count_pre_file_bytes);
    strputf(&buf, "|   %'11lu macro expansions\n", (unsigned long)g_metrics.count_macro_expansions);
    strputf(&buf, "|-Parse: %.2fms\n", AS_MSECS(g_metrics.parse_time + g_metrics.attribute_time));
    strputf(&buf, "| |-Types:      %.2fms\n", AS_MSECS(g_metrics.parse_time));
    strputf(&buf, "| |-Attributes: %.2fms\n", AS_MSECS(g_metrics.attribute_time));
    strputf(&buf, "|   %'11lu tokens\n", (unsigned long)g_metrics.count_parse_tokens);
    strputf(&buf, "|   %'11lu types\n", (unsigned long)g_metrics.count_parse_types);
    strputf(&buf, "|-Gen: %.2fms\n", AS_MSECS(g_metrics.gen_time));
    strputf(&buf, "|   %'11lu types\n", (unsigned long)g_metrics.count_gen_types);
    fputs(buf, stderr);
    arrfree(buf);
#undef AS_MSECS
}

static void parse_error(ParseContext * ctx, Token tk, char * message);
static void preprocess_message_internal(LocationContext * lctx, const Token * tk, char * message, int msg_type);

#endif // GLOBAL_C
