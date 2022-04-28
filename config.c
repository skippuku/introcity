typedef struct {
    const char ** sys_include_paths;
    const char * defines;
} Config;

void
generate_config(int argc, char ** argv, Config * cfg) {
    const char * compiler = NULL;
    const char * output_file = NULL;
    for (int i=0; i < argc; i++) {
        if (strcmp(argv[i], "--compiler")) {
            compiler = argv[++i];
        } else if (strcmp(argv[i], "--file")) {
            output_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option for gen-config: %s\n", argv[i]);
            exit(1);
        }
    }

    assert(cfg != NULL);
    memset(cfg, 0, sizeof(*cfg));

    if (compiler == NULL || strcmp(compiler, "") == 0) {
        fprintf(stderr, "No compiler specified.");
    }

    char cmd [1024];
    stbsp_snprintf(cmd, sizeof(cmd), "echo | %s -E -Wp,-v -", compiler);

    FILE * fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Failed to run compiler.");
        exit(1);
    }

    char line [1024];
    while (fgets(line, sizeof(line), fp)) {
        if (0==strcmp(line, "#include <...> search starts here:\n")) {
            while (fgets(line, sizeof(line), fp)) {
                if (0==strcmp(line, "End of search list.\n")) {
                    break;
                }
                char * s = line;
                while (is_space(*s)) s++;
                int length = strchr(s, '\n') - s;
                const char * path = copy_and_terminate(s, length);
                arrput(cfg->sys_include_paths, path);
            }
        }
    }

    stbsp_snprintf(cmd, sizeof(cmd), "echo | %s -E -Wp,-v -", compiler);
}
