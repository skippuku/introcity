#include <limits.h>

bool
get_config_path(char * o_path, const char * exe_dir) {
    char temp [4096];
    MemArena * arena = new_arena(1024);
    char ** paths = NULL;
    arrput(paths, "intro.cfg");
    arrput(paths, ".intro.cfg");

#if defined(__linux__) || defined(BSD) || defined(MSYS2_PATHS)
  #define LINUX_PATHS 1
#endif

#define ADD_PATH() do{ \
    strcat(temp, "/intro.cfg"); \
    path_normalize(temp); \
    arrput(paths, copy_and_terminate(arena, temp, strlen(temp))); \
} while(0)

#ifdef LINUX_PATHS
    char * configdir = getenv("XDG_CONFIG_HOME");
    if (configdir) {
        strcpy(temp, configdir);
        strcat(temp, "/introcity");
        ADD_PATH();
    } else {
        char * home = getenv("HOME");
        if (home) {
            strcpy(temp, home);
            strcat(temp, "/.config/introcity");
            ADD_PATH();
        }
    }
#endif
#ifdef _WIN32
    HRESULT ret = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, temp);
    if (ret == S_OK) {
        strcat(temp, "/introcity");
        ADD_PATH();
    }
#endif

    if (0 != strcmp(exe_dir, ".")) {
        strcpy(temp, exe_dir);
        ADD_PATH();
    }

#ifdef LINUX_PATHS
    strcpy(temp, "/etc/introcity");
    ADD_PATH();
#endif
#undef ADD_PATH
#undef LINUX_PATHS

    bool ok = false;
    for (int i=0; i < arrlen(paths); i++) {
        char * path = paths[i];
        if (access(path, F_OK) == 0) {
            strcpy(o_path, path);
            ok = true;
            break;
        }
    }

    arrfree(paths);
    free_arena(arena);
    return ok;
}

Config
load_config(const char * buf) {
    Config cfg = {0};
    cfg.arena = new_arena(1024);
    
    const char * s = buf;
    while (*s != 0) {
        while (is_space(*s)) s++;
        if (0==memcmp(s, "[#", 2)) {
            s += 2;
            char section_name [256];
            const char * close = strchr(s, ']');
            if (!close || close - s >= LENGTH(section_name)) {
                fprintf(stderr, "Invalid config file.\n");
                exit(1);
            }
            memcpy(section_name, s, close - s);
            section_name[close - s] = 0;

            s = close;
            while (is_space(*++s));
            if (0==strcmp(section_name, "include paths")) {
                while (1) {
                    if (0==memcmp(s, "[#", 2)) break;
                    const char * end = strchr(s, '\n');
                    if (!end) break;
                    if (end - s <= 0) break;
                    char buf [1024];
                    strncpy(buf, s, sizeof(buf));
                    buf[end - s] = 0;
                    path_normalize(buf);
                    char * path = copy_and_terminate(cfg.arena, buf, strlen(buf));
                    arrput(cfg.sys_include_paths, path);
                    s = end;
                    while (is_space(*++s));
                }
            } else if (0==strcmp(section_name, "defines")) {
                const char * start = s;
                while (s && *s && 0 != memcmp(s, "\n[", 2)) s = strchr(s, '\n') + 1;
                const char * end = s;
                char * result = malloc(end - start + 1);
                memcpy(result, start, end - start);
                result[end - start] = 0;
                cfg.defines = result;
            } else if (0==strcmp(section_name, "types")) {
                while (1) {
                    if (0==memcmp(s, "[#", 2)) break;
                    const char * start = s;
                    const char * end = strchr(s, '=');
                    const char * v_start = end + 1;
                    while (*--end == ' ');
                    while (*++v_start == ' ');
                    int len = end - start;
                    char * v_end = NULL;
                    uint8_t value = (uint8_t)strtol(v_start, &v_end, 10);
                    s = v_end;
                    while (is_space(*++s));
                    if (0==strncmp(start, "void *", len)) {
                        cfg.type_info.size_ptr = value;
                    } else if (0==strncmp(start, "short", len)) {
                        cfg.type_info.size_short = value;
                    } else if (0==strncmp(start, "int", len)) {
                        cfg.type_info.size_int = value;
                    } else if (0==strncmp(start, "long", len)) {
                        cfg.type_info.size_long = value;
                    } else if (0==strncmp(start, "long long", len)) {
                        cfg.type_info.size_long_long = value;
                    } else if (0==strncmp(start, "long double", len)) {
                        cfg.type_info.size_long_double = value;
                    } else if (0==strncmp(start, "_Bool", len)) {
                        cfg.type_info.size_bool = value;
                    } else if (0==strncmp(start, "char_is_signed", len)) {
                        cfg.type_info.char_is_signed = value;
                    } else {
                        fprintf(stderr, "Invalid config file.\n");
                        exit(1);
                    }
                }
            } else {
                fprintf(stderr, "Invalid config file.\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "Invalid config file.\n");
            exit(1);
        }
    }

    return cfg;
}

void
generate_config(int argc, char ** argv) {
    // At the moment, this will probably only work with gcc and clang

    Config cfg = {0};
    MemArena * arena = new_arena(1024);

    const char * compiler = NULL;
    const char * output_file = NULL;
    for (int i=0; i < argc; i++) {
        if (0==strcmp(argv[i], "--compiler")) {
            compiler = argv[++i];
        } else if (0==strcmp(argv[i], "--file")) {
            output_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option for gen-config: %s\n", argv[i]);
            exit(1);
        }
    }

    if (compiler == NULL || strcmp(compiler, "") == 0) {
        fprintf(stderr, "No compiler specified.");
        exit(1);
    }

    char cmd [1024];
    stbsp_snprintf(cmd, sizeof(cmd), "echo | %s -std=c99 -E -Wp,-v - 2>&1", compiler);

    FILE * fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Failed to run command.");
        exit(1);
    }

    char line [1024];
    while (fgets(line, sizeof(line), fp)) {
        bool break_outer = false;
        if (0==strcmp(line, "#include <...> search starts here:\n")) {
            while (fgets(line, sizeof(line), fp)) {
                if (0==strcmp(line, "End of search list.\n")) {
                    break_outer = true;
                    break;
                }
                char * s = line;
                while (is_space(*s)) s++;
                int length = strchr(s, '\n') - s;
                char * path = copy_and_terminate(arena, s, length);
                arrput(cfg.sys_include_paths, path);
            }
            if (break_outer) break;
        }
    }
    pclose(fp);

    stbsp_snprintf(cmd, sizeof(cmd), "echo | %s -std=c99 -E -dM -", compiler);
    fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Failed to run command.");
        exit(1);
    }
    cfg.defines = read_stream(fp);
    pclose(fp);

    cfg.type_info = (CTypeInfo){
        .size_ptr = sizeof(void *),
        .size_short = sizeof(short),
        .size_int = sizeof(int),
        .size_long = sizeof(long),
        .size_long_long = sizeof(long long),
        .size_long_double = sizeof(long double),
        .size_bool = sizeof(_Bool),
        .char_is_signed = CHAR_MIN < 0,
    };

    char * s = NULL;

    strputf(&s, "[#include paths]\n");
    for (int i=0; i < arrlen(cfg.sys_include_paths); i++) {
        strputf(&s, "%s\n", cfg.sys_include_paths[i]);
    }

    strputf(&s, "\n[#types]\n");
    strputf(&s, "void * = %u\n", cfg.type_info.size_ptr);
    strputf(&s, "short = %u\n", cfg.type_info.size_short);
    strputf(&s, "int = %u\n", cfg.type_info.size_int);
    strputf(&s, "long = %u\n", cfg.type_info.size_long);
    strputf(&s, "long long = %u\n", cfg.type_info.size_long_long);
    strputf(&s, "long double = %u\n", cfg.type_info.size_long_double);
    strputf(&s, "_Bool = %u\n", cfg.type_info.size_bool);
    strputf(&s, "char_is_signed = %u\n", cfg.type_info.char_is_signed);

    strputf(&s, "\n[#defines]\n");
    strputf(&s, "%s", cfg.defines);

    if (!output_file) {
        output_file = "intro.cfg";
    }
    int ret = intro_dump_file(output_file, s, arrlen(s));
    if (ret != 0) {
        fprintf(stderr, "Failed to write to file '%s'.\n", output_file);
        exit(1);
    }

    arrfree(cfg.defines);
    free_arena(arena);

    fprintf(stderr, "Generated intro config at '%s'.\n", output_file);
}
