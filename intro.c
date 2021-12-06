#include "basic.h"
#include "lexer.c"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define STB_INCLUDE_IMPLEMENTATION
#include "stb_include.h"

#include "intro.h"

#define strput(a,v) memcpy(arraddnptr(a, strlen(v)), v, strlen(v))
#define strputn(a,v,n) memcpy(arraddnptr(a, n), v, n)
#define strputnull(a) arrput(a,0)

#define BOLD_RED "\e[1;31m"
#define WHITE "\e[0;37m"

size_t
fsize(FILE * file) {
    long location = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t result = ftell(file);
    fseek(file, location, SEEK_SET);
    return result;
}

char *
read_and_allocate_file(const char * filename) {
    FILE * file = fopen(filename, "rb");
    assert(file != NULL);
    size_t file_size = fsize(file);
    char * buffer = malloc(file_size + 1);
    if (fread(buffer, file_size, 1, file) != 1) {
        fclose(file);
        free(buffer);
        return NULL;
    }
    fclose(file);
    buffer[file_size] = '\0';
    return buffer;
}

struct name_set_s {
    char * key;
} * name_set = NULL;

struct type_set_s {
    IntroType key;
    IntroType * value;
} * type_set = NULL;

static const char * IntroCategory_strings [INTRO_TYPE_COUNT] = {
    "INTRO_UNKNOWN",

    "INTRO_FLOATING",
    "INTRO_SIGNED",
    "INTRO_UNSIGNED",

    "INTRO_STRUCT",
    "INTRO_ENUM",
    "INTRO_ARRAY",
};

typedef struct KnownType {
    char * key;
    int value;
    IntroCategory category;
    union {
        IntroStruct * i_struct;
        IntroEnum * i_enum;
    };
} KnownType;

static const KnownType type_list [] = {
    {"uint8_t", 1, INTRO_UNSIGNED}, {"uint16_t", 2, INTRO_UNSIGNED}, {"uint32_t", 4, INTRO_UNSIGNED}, {"uint64_t", 8, INTRO_UNSIGNED},
    {"int8_t", 1, INTRO_SIGNED}, {"int16_t", 2, INTRO_SIGNED}, {"int32_t", 4, INTRO_SIGNED}, {"int64_t", 8, INTRO_SIGNED},
    {"size_t", sizeof(size_t), INTRO_UNSIGNED}, {"ptrdiff_t", sizeof(ptrdiff_t), INTRO_SIGNED},

    {"bool", sizeof(bool), INTRO_UNSIGNED}, {"char", 1, INTRO_SIGNED}, {"unsigned char", 1, INTRO_UNSIGNED},
    {"short", sizeof(short), INTRO_SIGNED}, {"int", sizeof(int), INTRO_SIGNED}, {"long", sizeof(long), INTRO_SIGNED},
    {"unsigned short", sizeof(short), INTRO_UNSIGNED}, {"unsigned int", sizeof(int), INTRO_UNSIGNED}, {"unsigned long", sizeof(long), INTRO_UNSIGNED},
    {"float", 4, INTRO_FLOATING}, {"double", 8, INTRO_FLOATING},
};

KnownType * known_types = NULL;

char *
copy_and_terminate(char * str, int length) {
    char * result = malloc(length + 1);
    memcpy(result, str, length);
    result[length] = '\0';
    return result;
}

int
get_line(char * begin, char * pos, char ** o_start_of_line) {
    char * s = begin;
    char * last_line;
    int line_num = 1;
    while (s < pos) {
        if (*s++ == '\n') {
            last_line = s;
            line_num++;
        }
    }
    if (o_start_of_line) *o_start_of_line = last_line;
    return line_num;
}

void
parse_error_internal(char * buffer, Token * tk, char * message) {
    char * start_of_line;
    int line_num = get_line(buffer, tk->start, &start_of_line);
    char * end_of_line = strchr(tk->start + tk->length, '\n') + 1;
    printf("Error (line %i): %s\n\n", line_num, message ? message : "Failed to parse.");
    printf("%.*s", (int)(tk->start - start_of_line), start_of_line);
    printf(BOLD_RED);
    printf("%.*s", tk->length, tk->start);
    printf(WHITE);
    printf("%.*s", (int)(end_of_line - (tk->start + tk->length)), tk->start + tk->length);
    for (int i=0; i < (tk->start - start_of_line); i++) putc(' ', stdout);
    for (int i=0; i < (tk->length); i++) putc('~', stdout);
    printf("\n");
}
#define parse_error(tk,message) parse_error_internal(buffer, tk, message)

static bool
tk_equal(Token * tk, const char * str) {
    size_t len = strlen(str);
    return (tk->length == len && memcmp(tk->start, str, len) == 0);
}

IntroStruct ** structs = NULL;
IntroEnum   ** enums = NULL;

int
parse_struct(char * buffer, char ** o_s) {
    IntroStruct struct_ = {0};

    Token tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        char * temp = copy_and_terminate(tk.start, tk.length);
        shputs(name_set, (struct name_set_s){temp});
        struct_.name = shgets(name_set, temp).key;
        free(temp);
        tk = next_token(o_s);
    }

    if (!(tk.type == TK_BRACE && tk.is_open)) {
        parse_error(&tk, "Expected open brace here.");
        return 1;
    }

    IntroMember * members = NULL;
    while (1) {
        IntroMember member = {0};

        Token * line_tokens = NULL;
        Token line_tk;
        while ((line_tk = next_token(o_s)).type == TK_IDENTIFIER || line_tk.type == TK_STAR || line_tk.type == TK_BRACKET) {
            arrput(line_tokens, line_tk);
        }
        if (line_tk.type == TK_BRACE && !line_tk.is_open) {
            break;
        }

        if (arrlast(line_tokens).type != TK_IDENTIFIER) {
            Token * t = &arrlast(line_tokens);
            parse_error(t, "Cannot parse symbol in member declaration.");
            return 1;
        }
        char * type_name = NULL;
        IntroType type = {0};
        for (int i=0; i < arrlen(line_tokens) - 1; i++) {
            Token * tk = &line_tokens[i];
            if (tk->type == TK_STAR) {
                type.pointer_level++;
            } else if (tk->type == TK_IDENTIFIER
                       && !tk_equal(tk, "const")
                       && !tk_equal(tk, "static")) {
                if (arrlen(type_name) != 0) strput(type_name, " ");
                strputn(type_name, tk->start, tk->length);
            }
        }

        strputnull(type_name);
        shputs(name_set, (struct name_set_s){type_name});
        type.name = shgets(name_set, type_name).key;
        arrfree(type_name);

        KnownType * kt = shgetp_null(known_types, type.name);
        if (kt != NULL) {
            type.size = type.pointer_level > 0 ? sizeof(void *) : kt->value;
            type.category = kt->category;
            if (kt->i_struct) type.i_struct = kt->i_struct;
        } else {
            Token hack = line_tokens[0];
            Token * last = &line_tokens[arrlen(line_tokens) - 2];
            hack.length = last->start + last->length - hack.start;
            char * error_str = NULL;
            strput(error_str, "Unknown type \"");
            strput(error_str, type.name);
            strput(error_str, "\".");
            strputnull(error_str);
            parse_error(&hack, error_str);
            return 1;
        }

        if (hmgeti(type_set, type) >= 0) {
            member.type = hmget(type_set, type);
        } else {
            IntroType * stored = malloc(sizeof(IntroType));
            memcpy(stored, &type, sizeof(IntroType));
            hmput(type_set, type, stored);
            member.type = stored;
        }

        Token * member_name_tk = &arrlast(line_tokens);
        char * temp = copy_and_terminate(member_name_tk->start, member_name_tk->length);
        shputs(name_set, (struct name_set_s){temp});
        member.name = shgets(name_set, temp).key;
        free(temp);

        arrput(members, member);

        arrfree(line_tokens);
    }

    struct_.count_members = arrlen(members);

    IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
    memcpy(result, &struct_, sizeof(IntroStruct));
    memcpy(result->members, members, sizeof(IntroMember) * arrlen(members));
    arrfree(members);

    if (struct_.name != NULL) {
        char * struct_type_name = NULL;
        strput(struct_type_name, "struct ");
        strput(struct_type_name, result->name);
        strputnull(struct_type_name);

        KnownType struct_type;
        struct_type.key = struct_type_name;
        struct_type.value = 0;
        struct_type.category = INTRO_STRUCT;
        struct_type.i_struct = result;
        shputs(known_types, struct_type);

        arrfree(struct_type_name);
    }

    arrput(structs, result);

    return 0;
}

int
parse_enum(char * buffer, char ** o_s) {
    IntroEnum enum_ = {0};

    Token tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        char * temp = copy_and_terminate(tk.start, tk.length);
        shputs(name_set, (struct name_set_s){temp});
        enum_.name = shgets(name_set, temp).key;
        free(temp);
        tk = next_token(o_s);
    }

    if (!(tk.type == TK_BRACE && tk.is_open)) {
        parse_error(&tk, "Expected open brace here.");
        return 1;
    }

    enum_.is_flags = true;
    enum_.is_sequential = true;
    IntroEnumValue * members = NULL;
    int next_int = 0;
    int mask = 0;
    while (1) {
        IntroEnumValue v = {0};
        Token name = next_token(o_s);
        if (name.type == TK_BRACE && !name.is_open) {
            break;
        }
        if (name.type != TK_IDENTIFIER) {
            parse_error(&tk, "Expected identifier.");
            return 1;
        }

        char * new_name = copy_and_terminate(name.start, name.length);
        if (shgeti(name_set, new_name) >= 0) {
            parse_error(&name, "Cannot define enumeration with reserved name.");
            return 1;
        }
        shputs(name_set, (struct name_set_s){new_name});
        v.name = shgets(name_set, new_name).key;

        tk = next_token(o_s);
        bool set = false;
        bool is_last = false;
        if (tk.type == TK_COMMA) {
            v.value = next_int++;
        } else if (tk.type == TK_EQUAL) {
            long num = strtol(*o_s, o_s, 0);
            if (num == 0 && errno != 0) {
                parse_error(&tk, "Unable to parse enumeration value.");
                return 1;
            }
            v.value = (int)num;
            if (v.value != next_int) {
                enum_.is_sequential = false;
            }
            next_int = v.value + 1;
            set = true;
        } else if (tk.type == TK_BRACE && !tk.is_open) {
            v.value = next_int;
            is_last = true;
        } else {
            parse_error(&tk, "Unexpected symbol.");
            return 1;
        }

        if (mask & v.value) enum_.is_flags = false;
        mask |= v.value;

        arrput(members, v);

        if (is_last) break;

        if (set) {
            tk = next_token(o_s);
            if (tk.type == TK_COMMA) {
            } else if (tk.type == TK_BRACE && !tk.is_open) {
                break;
            } else {
                parse_error(&tk, "Unexpected symbol.");
                return 1;
            }
        }
    };

    enum_.count_members = arrlen(members);

    IntroEnum * result = malloc(sizeof(IntroEnum) + sizeof(*members) * arrlen(members));
    memcpy(result, &enum_, sizeof(IntroEnum));
    memcpy(result->members, members, sizeof(*members) * arrlen(members));
    arrfree(members);

    if (enum_.name != NULL) {
        char * enum_type_name = NULL;
        strput(enum_type_name, "enum ");
        strput(enum_type_name, enum_.name);
        strputnull(enum_type_name);

        KnownType enum_type;
        enum_type.key = enum_type_name;
        enum_type.value = 0;
        enum_type.category = INTRO_ENUM;
        enum_type.i_enum = result;
        shputs(known_types, enum_type);

        arrfree(enum_type_name);
    }

    arrput(enums, result);

    return 0;
}

int
parse_typedef(char * buffer, char ** o_s) {
    Token * line_tokens = NULL;
    Token line_tk;
    bool is_anonymous_struct = false;
    bool is_anonymous_enum = false;
    while ((line_tk = next_token(o_s)).type == TK_IDENTIFIER || line_tk.type == TK_STAR) {
        arrput(line_tokens, line_tk);
        if (tk_equal(&line_tk, "struct")) {
            char * after_struct_keyword = *o_s;
            int error = parse_struct(buffer, o_s);
            if (error) return error;
            if (arrlast(structs)->name == NULL) {
                is_anonymous_struct = true;
            } else {
                arrput(line_tokens, next_token(&after_struct_keyword));
            }
        } else if (tk_equal(&line_tk, "enum")) {
            char * after_enum_keyword = *o_s;
            int error = parse_enum(buffer, o_s);
            if (error) return error;
            if (arrlast(enums)->name == NULL) {
                is_anonymous_enum = true;
            } else {
                arrput(line_tokens, next_token(&after_enum_keyword));
            }
        }
    }
    if (line_tk.type != TK_SEMICOLON) {
        parse_error(&line_tk, "Cannot parse symbol in typedef. Expected ';'");
        return 1;
    }
    if (arrlast(line_tokens).type != TK_IDENTIFIER) {
        parse_error(&arrlast(line_tokens), "Cannot parse symbol in typedef. Expected identifier.");
        return 1;
    }
    Token * last = &arrlast(line_tokens);
    char * new_type_name = copy_and_terminate(last->start, last->length);
    if (shgeti(known_types, new_type_name) >= 0) {
        parse_error(last, "Cannot define a type with this name. The name is already reserved.");
    }

    if (is_anonymous_struct) {
        IntroStruct * i_struct = arrlast(structs);
        KnownType k = {0};
        k.key = new_type_name;
        k.category = INTRO_STRUCT;
        k.i_struct = i_struct;
        shputs(known_types, k);

        i_struct->name = shgets(known_types, new_type_name).key;
    } else if (is_anonymous_enum) {
        IntroEnum * i_enum = arrlast(enums);
        KnownType k = {0};
        k.key = new_type_name;
        k.category = INTRO_ENUM;
        k.i_enum = i_enum;
        shputs(known_types, k);

        i_enum->name = shgets(known_types, new_type_name).key;
    } else {
        char * type_name = NULL;
        for (int i=0; i < arrlen(line_tokens) - 1; i++) {
            Token * tk = &line_tokens[i];
            if (tk->type == TK_STAR) {
                parse_error(tk, "Pointers are not currently supported in typedefs.");
            } else if (tk->type == TK_IDENTIFIER
                       && !tk_equal(tk, "const")
                       && !tk_equal(tk, "static")) {
                if (arrlen(type_name) != 0) strput(type_name, " ");
                strputn(type_name, tk->start, tk->length);
            } else {
                parse_error(tk, "Cannot parse symbol in typedef.");
                return 1;
            }
        }
        strputnull(type_name);

        KnownType * kt = shgetp_null(known_types, type_name);
        if (kt != NULL) {
            KnownType nt = *kt;
            nt.key = new_type_name;
            shputs(known_types, nt);
        } else {
            Token hack = line_tokens[0];
            hack.length = last->start + last->length - hack.start;
            parse_error(&hack, "Unknown type in typedef.");
            for (int i=0; i < arrlen(line_tokens); i++) {
                printf("%.*s\n", line_tokens[i].length, line_tokens[i].start);
            }
            return 1;
        }
        arrfree(type_name);
    }

    free(new_type_name);

    return 0;
}

int
main(int argc, char ** argv) {
    if (argc != 2) {
        printf("incorrect number of arguments, aborting\n");
        return 1;
    }

    char * header_filename = argv[1];
    char stb_include_error [256];
    char * buffer = stb_include_file(header_filename, "", ".", stb_include_error);
    if (buffer == NULL) {
        printf("Include Error:\n%s\n", stb_include_error);
        return 1;
    }
    char * s = buffer;

    sh_new_arena(known_types);
    sh_new_arena(name_set);

    for (int i=0; i < LENGTH(type_list); i++) {
        shputs(known_types, type_list[i]);
    }

    Token key;
    while ((key = next_token(&s)).type != TK_END) {
        if (key.type == TK_IDENTIFIER) {
            if (tk_equal(&key, "struct")) {
                int error = parse_struct(buffer, &s);
                if (error) return error;
            } else if (tk_equal(&key, "enum")) {
                int error = parse_enum(buffer, &s);
                if (error) return error;
            } else if (tk_equal(&key, "typedef")) {
                int error = parse_typedef(buffer, &s);
                if (error) return error;
            }
        } else if (key.type == TK_HASH) {
            Token directive = next_token(&s);
            // NOTE: does not handle #elif or #else after an #if 1
            // TODO(cy): handle this with the preprocessor instead
            // replace the stb one with our own
            if (tk_equal(&directive, "if")) { // handle #if 0
                Token value = next_token(&s);
                if (value.length == 1 && *value.start == '0') {
                    while (1) {
                        char * h = strchr(s, '#');
                        if (h == NULL) break;
                        s = h + 1;
                        directive = next_token(&s);
                        if (directive.type == TK_END
                            || tk_equal(&directive, "endif")
                            || tk_equal(&directive, "else")) {
                            break;
                        }
                    }
                }
            } else { // ignore directive
                while (1) {
                    while (*s != '\n' && *s != '\0') s++;
                    if (*s == '\0') break;
                    char * q = s;
                    while (isspace(*--q));
                    if (*q != '\\') break;
                    s++;
                }
            }
        }
    }

    char num_buf [64];
    char * str = NULL;

    strput(str, "\nstruct {\n");
    strput(str, "\tIntroType types [");
    sprintf(num_buf, "%i", (int)hmlen(type_set));
    strput(str, num_buf);
    strput(str, "];\n");
    for (int enum_index = 0; enum_index < arrlen(enums); enum_index++) {
        if (enums[enum_index]->name == NULL) continue;
        strput(str, "\tIntroEnum * ");
        strput(str, enums[enum_index]->name);
        strput(str, ";\n");
    }
    for (int struct_index = 0; struct_index < arrlen(structs); struct_index++) {
        if (structs[struct_index]->name == NULL) continue;
        strput(str, "\tIntroStruct * ");
        strput(str, structs[struct_index]->name);
        strput(str, ";\n");
    }
    strput(str, "} intro_data;\n\n");

    strput(str, "void\n" "intro_init() {\n");

    strput(str, "\t// CREATE ENUM INTROSPECTION DATA\n");
    strput(str, "\n\tIntroEnumValue * v = NULL;\n");
    for (int enum_index = 0; enum_index < arrlen(enums); enum_index++) {
        IntroEnum * e = enums[enum_index];

        strput(str, "\n\t// ");
        strput(str, e->name);
        strput(str, "\n");

        strput(str, "\n\tintro_data.");
        strput(str, e->name);
        strput(str, " = malloc(sizeof(IntroEnum) + ");
        sprintf(num_buf, "%i", e->count_members);
        strput(str, num_buf);
        strput(str, " * sizeof(IntroEnumValue));\n");

        strput(str, "\tintro_data.");
        strput(str, e->name);
        strput(str, "->name = \"");
        strput(str, e->name);
        strput(str, "\";\n");

        strput(str, "\tintro_data.");
        strput(str, e->name);
        strput(str, "->is_flags = ");
        strput(str, e->is_flags ? "true" : "false");
        strput(str, ";\n");

        strput(str, "\tintro_data.");
        strput(str, e->name);
        strput(str, "->is_sequential = ");
        strput(str, e->is_sequential ? "true" : "false");
        strput(str, ";\n");

        strput(str, "\tintro_data.");
        strput(str, e->name);
        strput(str, "->count_members = ");
        sprintf(num_buf, "%u", e->count_members);
        strput(str, num_buf);
        strput(str, ";\n");

        strput(str, "\tv = intro_data.");
        strput(str, e->name);
        strput(str, "->members;\n");

        for (int i=0; i < e->count_members; i++) {
            char v_buf [64];
            sprintf(v_buf, "\tv[%i].", i);
            IntroEnumValue * v = &e->members[i];
            
            strput(str, "\n");

            strput(str, v_buf);
            strput(str, "name = \"");
            strput(str, v->name);
            strput(str, "\";\n");

            strput(str, v_buf);
            strput(str, "value = ");
            sprintf(num_buf, "%i", v->value);
            strput(str, num_buf);
            strput(str, ";\n");
        }
    }

    strput(str, "\n\t// CREATE STRUCT INTROSPECTION DATA\n");
    strput(str, "\n\tIntroMember * m = NULL;\n");
    for (int struct_index=0; struct_index < arrlen(structs); struct_index++) {
        IntroStruct * s = structs[struct_index];

        strput(str, "\n\t// ");
        strput(str, s->name);
        strput(str, "\n");

        strput(str, "\n\tintro_data.");
        strput(str, s->name);
        strput(str, " = malloc(sizeof(IntroStruct) + ");
        sprintf(num_buf, "%i", s->count_members);
        strput(str, num_buf);
        strput(str, " * sizeof(IntroMember));\n");

        strput(str, "\tintro_data.");
        strput(str, s->name);
        strput(str, "->name = \"");
        strput(str, s->name);
        strput(str, "\";\n");

        strput(str, "\tintro_data.");
        strput(str, s->name);
        strput(str, "->count_members = ");
        sprintf(num_buf, "%u", s->count_members);
        strput(str, num_buf);
        strput(str, ";\n");

        strput(str, "\tm = intro_data.");
        strput(str, s->name);
        strput(str, "->members;\n");

        for (int i=0; i < s->count_members; i++) {
            char m_buf [64];
            sprintf(m_buf, "\tm[%i].", i);
            IntroMember * m = &s->members[i];

            strput(str, "\n");

            strput(str, m_buf);
            strput(str, "name = \"");
            strput(str, m->name);
            strput(str, "\";\n");

            strput(str, m_buf);
            strput(str, "type = &intro_data.types[");
            sprintf(num_buf, "%i", (int)hmgeti(type_set, *m->type));
            strput(str, num_buf);
            strput(str, "];\n");

            strput(str, m_buf);
            strput(str, "offset = offsetof(");
            strput(str, s->name);
            strput(str, ", ");
            strput(str, m->name);
            strput(str, ");\n");
        }
    }

    strput(str, "\n\t// CREATE TYPES\n\n");
    strput(str, "\tIntroType * t = intro_data.types;\n\n");
    for (int type_index = 0; type_index < hmlen(type_set); type_index++) {
        char t_buf [64];
        sprintf(t_buf, "\tt[%i].", type_index);

        IntroType * t = type_set[type_index].value;

        strput(str, t_buf);
        strput(str, "name = \"");
        strput(str, t->name);
        strput(str, "\";\n");

        strput(str, t_buf);
        strput(str, "size = ");
        if (t->size) {
            sprintf(num_buf, "%u", t->size);
            strput(str, num_buf);
        } else {
            strput(str, "sizeof(");
            strput(str, t->name);
            strput(str, ")");
        }
        strput(str, ";\n");

        strput(str, t_buf);
        strput(str, "category = ");
        strput(str, IntroCategory_strings[t->category]);
        strput(str, ";\n");

        strput(str, t_buf);
        strput(str, "pointer_level = ");
        sprintf(num_buf, "%u", t->pointer_level);
        strput(str, num_buf);
        strput(str, ";\n");

        if (t->category == INTRO_STRUCT) {
            strput(str, t_buf);
            strput(str, "i_struct = intro_data.");
            strput(str, t->i_struct->name);
            strput(str, ";\n");
        } else if (t->category == INTRO_ENUM) {
            strput(str, t_buf);
            strput(str, "i_enum = intro_data.");
            strput(str, t->i_enum->name);
            strput(str, ";\n");
        }

        strput(str, "\n");
    }
    strput(str, "}\n\n");

    strput(str, "void\n");
    strput(str, "intro_uninit() {\n");
    for (int struct_index = 0; struct_index < arrlen(structs); struct_index++) {
        strput(str, "\tfree(intro_data.");
        strput(str, structs[struct_index]->name);
        strput(str, ");\n");
    }
    for (int enum_index = 0; enum_index < arrlen(enums); enum_index++) {
        strput(str, "\tfree(intro_data.");
        strput(str, enums[enum_index]->name);
        strput(str, ");\n");
    }
    strput(str, "}\n");

    strputnull(str);

    char save_filename_buffer [128];
    sprintf(save_filename_buffer, "%s.intro", header_filename);
    FILE * save_file = fopen(save_filename_buffer, "w");
    fprintf(save_file, str);
    fclose(save_file);

#ifdef DEBUG
    arrfree(str);
    shfree(name_set);
    hmfree(type_set);
    free(buffer);
    for (int i=0; i < arrlen(structs); i++) {
        free(structs[i]);
    }
    arrfree(structs);
    shfree(known_types);
#endif
}

// TODO LAND

/*
FIX: parse_error line number is incorrect because of preprocessor
    + show which file the error is in

Handle forward declarations

Anonymous structs

Unions (special structs)

Array types
    how should multidimentional arrays be handled?

Preprocessor
    Should we use an existing one?
        CON: No invisible macros

Ignore functions
    should this be done in the preprocessor?

Serialization

User data (with macros)
    enum flags
    versions
    custom type data

Program arguments
    create typedefs for structs and enums
*/

#if 0
bool
intro_is_scalar(IntroType * type) {
    return type->pointer_level == 0 && (type->category == INTRO_FLOATING || type->category == INTRO_UNSIGNED || type->category == INTRO_SIGNED);
}

bool
intro_compatible(IntroType * a, IntroType * b) {
    if (intro_is_scalar(a)) {
        if (intro_is_scalar(b)) {
            return (a->pointer_level == b->pointer_level
                 && a->category == b->category
                 && a->size == b->size);
        } else {
            return false;
        }
    } else {
        if (!intro_is_scalar(b)) {
            // TODO(cy): finish this
        } else {
            return false;
        }
    }
}
#endif
