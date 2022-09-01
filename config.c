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

void
load_config_file_data(Config * cfg, const char * buf) {
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
            assert(close - s < sizeof(section_name));
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
                    char * path = copy_and_terminate(cfg->arena, buf, strlen(buf));
                    arrput(cfg->sys_include_paths, path);
                    s = end;
                    while (is_space(*++s));
                }
            } else if (0==strcmp(section_name, "defines")) {
                const char * start = s;
                while (s && *s && 0 != memcmp(s, "\n[", 2)) s = strchr(s, '\n') + 1;
                const char * end = s;
                size_t buf_size = end - start;
                char * result = arena_alloc(cfg->arena, buf_size + 1);
                memcpy(result, start, buf_size);
                result[buf_size] = 0;
                cfg->defines = result;
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
                        cfg->type_info.size_ptr = value;
                    } else if (0==strncmp(start, "short", len)) {
                        cfg->type_info.size_short = value;
                    } else if (0==strncmp(start, "int", len)) {
                        cfg->type_info.size_int = value;
                    } else if (0==strncmp(start, "long", len)) {
                        cfg->type_info.size_long = value;
                    } else if (0==strncmp(start, "long long", len)) {
                        cfg->type_info.size_long_long = value;
                    } else if (0==strncmp(start, "long double", len)) {
                        cfg->type_info.size_long_double = value;
                    } else if (0==strncmp(start, "_Bool", len)) {
                        cfg->type_info.size_bool = value;
                    } else if (0==strncmp(start, "char_is_signed", len)) {
                        cfg->type_info.char_is_signed = value;
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

static const char help_dialog [] =
"intro - parser and introspection data generator\n"
"USAGE: intro [OPTIONS] file\n"
"\n"
"OPTIONS:\n"
" -o       specify output file\n"
" -        use stdin as input\n"
" --cfg    specify config file\n"
" -I -D -U -E -M -MP -MM -MD -MMD -MG -MT -MF (like gcc)\n"
" -MT_     output space separated dependency list with no target\n"
" -MTn     output newline separated dependency list with no target\n"
;

static const char *const filename_stdin = "__stdin__";

void interactive_calculator();

void
parse_program_arguments(Config * cfg, int argc, char * argv []) {
    if (argc > 1 && 0==strcmp(argv[1], "--calculator")) {
        interactive_calculator();
        exit(0);
    }
    if (argc > 1 && 0==strcmp(argv[1], "--gen-config")) {
        generate_config(argc - 2, &argv[2]);
        exit(0);
    }

    cfg->program_name = argv[0];

    for (int i=1; i < argc; i++) {
        #define ADJACENT() ((strlen(arg) == 2)? argv[++i] : arg+2)
        char * arg = argv[i];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case '-': {
                arg = argv[i] + 2;
                if (0==strcmp(arg, "cfg")) {
                    cfg->config_filename = argv[++i];
                } else if (0==strcmp(arg, "help")) {
                    fputs(help_dialog, stderr);
                    exit(0);
                } else if (0==strcmp(arg, "gen-city")) {
                    cfg->gen_city = true;
                } else if (0==strcmp(arg, "gen-vim-syntax")) {
                    cfg->gen_vim_syntax = true;
                } else if (0==strcmp(arg, "gen-typedefs")) {
                    cfg->gen_typedefs = true;
                } else if (0==strcmp(arg, "pragma")) {
                    char * text = argv[++i];
                    char * text_cpy = arena_alloc(cfg->arena, strlen(text) + 2);
                    strcpy(text_cpy, text);
                    strcat(text_cpy, "\n");
                    cfg->pragma = text_cpy;
                } else {
                    fprintf(stderr, "Unknown option: '%s'\n", arg);
                    exit(1);
                }
            }break;

            case 'h': {
                fputs(help_dialog, stderr);
                exit(0);
            }break;

            case 'D': {
                char * opt_str = ADJACENT();
                PreOption pre_op = {.type = PRE_OP_DEFINE, .string = opt_str};
                arrput(cfg->pre_options, pre_op);
            }break;

            case 'U': {
                char * opt_str = ADJACENT();
                PreOption pre_op = {.type = PRE_OP_UNDEFINE, .string = opt_str};
                arrput(cfg->pre_options, pre_op);
            }break;

            case 'I': {
                char * new_path = ADJACENT();
                arrput(cfg->include_paths, new_path);
            }break;

            case 'E': {
                cfg->pre_only = true;
            }break;

            case 'o': {
                cfg->output_filename = argv[++i];
            }break;

            case 'V': {
                cfg->show_metrics = true;
            }break;

            case 0: {
                if (isatty(fileno(stdin))) {
                    fprintf(stderr, "Error: Cannot use terminal as file input.\n");
                    exit(1);
                }
                PreOption pre_op = {.type = PRE_OP_INPUT_FILE, .string = (char *)filename_stdin};
                arrput(cfg->pre_options, pre_op);
            }break;

            case 'M': {
                switch(arg[2]) {
                case 0: {
                }break;

                case 'M': {
                    cfg->m_options.no_sys = true;
                    if (arg[3] == 'D') cfg->m_options.D = true;
                }break;

                case 'D': {
                    cfg->m_options.D = true;
                }break;

                case 'G': {
                    cfg->m_options.G = true;
                }break;

                case 'T': {
                    if (arg[3]) {
                        if (arg[3] == '_') {
                            cfg->m_options.target_mode = MT_SPACE;
                        } else if (arg[3] == 'n') {
                            cfg->m_options.target_mode = MT_NEWLINE;
                        } else {
                            goto unknown_option;
                        }
                        break;
                    } else {
                        cfg->m_options.target_mode = MT_NORMAL;
                        cfg->m_options.custom_target = argv[++i];
                    }
                }break;

                case 'F' :{
                    cfg->m_options.filename = argv[++i];
                }break;

                case 's': {
                    if (0==strcmp(arg+3, "ys")) { // -Msys
                        cfg->m_options.use_msys_path = true;
                    } else {
                        goto unknown_option;
                    }
                }break;

                case 'P': {
                    cfg->m_options.P = true;
                }break;

                default: goto unknown_option;
                }

                cfg->m_options.enabled = true;
            }break;

            unknown_option: {
                fprintf(stderr, "Error: Unknown option '%s'\n", arg);
                exit(1);
            }break;
            }
        } else {
            PreOption pre_op = {.type = PRE_OP_INPUT_FILE, .string = arg};
            arrput(cfg->pre_options, pre_op);
        }
        #undef ADJACENT
    }

    if (cfg->m_options.enabled && cfg->m_options.target_mode == MT_NEWLINE) {
        struct stat out_stat;
        fstat(fileno(stdout), &out_stat);
        if (S_ISFIFO(out_stat.st_mode) && !isatty(fileno(stdout))) {
            fprintf(stderr, "WARNING: Newline separation '-MTn' may cause unexpected behavior. Consider using space separation '-MT_'.\n");
        }
    }
    if (cfg->m_options.target_mode != MT_NORMAL && cfg->m_options.P) {
        fprintf(stderr, "Error: Cannot use '-MT_' or -'MTn' with '-MP'.\n");
        exit(1);
    }

    for (int i=0; i < arrlen(cfg->pre_options); i++) {
        PreOption pre_op = cfg->pre_options[i];
        if (pre_op.type == PRE_OP_INPUT_FILE) {
            cfg->first_input_filename = pre_op.string;
            break;
        }
    }

    if (!cfg->first_input_filename) {
        fprintf(stderr, "No input file.\n");
        exit(1);
    }
}

Config
get_config(int argc, char * argv []) {
    Config cfg = {0};
    cfg.arena = new_arena(1024);

    parse_program_arguments(&cfg, argc, argv);

    char path [4096];
    if (!cfg.config_filename) {
        char program_dir [4096];
        char program_path_norm [4096];
        strcpy(program_path_norm, argv[0]);
        path_normalize(program_path_norm);
        path_dir(program_dir, program_path_norm, NULL);
        if (get_config_path(path, program_dir)) {
            cfg.config_filename = path;
        }
    }

    const char * file_data = NULL;
    if (cfg.config_filename) {
        file_data = intro_read_file(cfg.config_filename, NULL);
    }

    if (file_data) {
        load_config_file_data(&cfg, file_data);
    } else {
        fprintf(stderr, "Could not find intro.cfg.\n");
        exit(1);
    }

    if (cfg.output_filename == NULL) {
        char * ext;
        if (cfg.gen_city) {
            ext = ".cty";
        } else if (cfg.gen_vim_syntax) {
            ext = ".vim";
        } else {
            ext = ".intro";
        }

        char * buf = arena_alloc(cfg.arena, strlen(cfg.first_input_filename) + strlen(ext) + 4);
        strcpy(buf, cfg.first_input_filename);
        strcat(buf, ext);
        cfg.output_filename = buf;
    }

    return cfg;
}
