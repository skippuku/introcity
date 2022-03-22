#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "intro.h"
#include "util.c"
#include "lexer.c"

typedef struct {
    char * buffer;
    struct{char * key; IntroType * value;} * type_map;
    struct{char * key;} * member_map;
} ParseContext;

static IntroType *
store_type(ParseContext * ctx, const IntroType * type) {
    IntroType * stored = malloc(sizeof(*stored));
    memcpy(stored, type, sizeof(*stored));
    shput(ctx->type_map, stored);
    ptrdiff_t index = shtemp(ctx->type_map);
    stored.name = ctx->type_map[index].key;
    return ctx->type_map[index].value;
}

void parse_error(ParseContext * ctx, Token * tk, char * message) { // TODO
}

IntroType * parse_base_type(ParseContext *, char **);
IntroType * parse_declaration(ParseContext *, char **, char **);

static int
parse_struct(ParseContext * ctx, char ** o_s) {
    Token tk = next_token(o_s), name_tk = {0};

    bool is_union;
    if (tk_equal(&tk, "struct")) {
        is_union = false;
    } else if (tk_equal(&tk, "union")) {
        is_union = true;
    } else {
        // TODO(print error)
        return -1;
    }

    tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        name_tk = tk;
        tk = next_token(o_s);
    }

    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR) return 2;
        parse_error(&tk, "Expected '{'.");
        return -1;
    }

    IntroMember * members = NULL;
    AttributeSpecifier * attribute_specifiers = NULL;
    int struct_index = arrlen(structs);
    arrput(structs, NULL);
    while (1) {
        IntroType * base_type = parse_base_type(ctx, o_s);
        if (!base_type) {
            Token tk = next_token(o_s);
            if (tk.type == TK_R_BRACE) {
                break;
            } else {
                return 1;
            }
        }

        Token tk = next_token(o_s);
        if (tk.type == TK_SEMICOLON) {
            if (type->category == INTRO_STRUCT) {
                IntroStruct * s = type->i_struct;
                for (int i=0; i < s->count_members; i++) {
                    arrput(members, s->members[i]);
                }
                #if 0 // TODO: how should this be handled?
                if (decl.is_nested) {
                    (void)arrpop(structs);
                }
                #endif
                continue;
            } else {
                parse_error(&tk, "Struct member has no name or type is unknown.");
                return 1;
            }
        } else {
            *o_s = tk.start;
            while (1) {
                IntroMember member = {0};

                IntroType * type = parse_declaration(buffer, o_s, &member.name);
                if (!type) return 1;

                if (type->category == INTRO_UNKNOWN) {
                    parse_error(&decl.type_tk, "Unknown type.");
                    return 1;
                }

                #if 0 // TODO: handle this
                if (decl.is_nested) {
                    struct nested_info_s info = {0};
                    info.key = type.i_struct;
                    info.struct_index = struct_index;
                    info.member_index = arrlen(members);
                    hmputs(nested_info, info);
                }
                #endif

                tk = next_token(o_s);
                int error = maybe_expect_attribute(buffer, o_s, arrlen(members), &tk, &attribute_specifiers);
                if (error) return error;

                arrput(members, member);

                if (tk.type == TK_SEMICOLON) {
                    break;
                } else if (tk.type == TK_COMMA) {
                } else {
                    parse_error(ctx, &tk, "Cannot parse symbol in member declaration. Expected ';' or ','.");
                    return 1;
                }
            }
        }
    }

    IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
    result.count_members = arrlen(members);
    result.is_union = is_union;
    memcpy(result->members, members, sizeof(IntroMember) * arrlen(members));
    arrfree(members);

    {
        char * struct_type_name = NULL;
        strputf(&struct_type_name, "%s ", is_union ? "union" : "struct");
        if (name_tk.type == TK_IDENTIFIER) {
            strputf(&struct_type_name, "%.*s", name_tk.length, name_tk.start);
        } else {
            static int anon_index = 0; // NOTE: not sure about this...
            strputf(&struct_type_name, "Anon_%i", anon_index++); 
        }
        strputnull(struct_type_name);

        IntroType type;
        type.name = struct_type_name;
        type.category = INTRO_STRUCT;
        type.i_struct = result;
        
        shput(ctx->type_map, &type);
        arrfree(struct_type_name);
#if 0 // TODO
        if (prev != NULL) {
            if (prev->category == INTRO_UNKNOWN) {
                for (int i=0; i < arrlen(prev->forward_list); i++) {
                    KnownType * ft = &known_types[prev->forward_list[i]];
                    ft->category = INTRO_STRUCT;
                    ft->i_struct = result;
                }
                arrfree(prev->forward_list);
            } else {
                parse_error(&name_tk, "Redefinition of type.");
                return 1;
            }
        }
        shputs(known_types, struct_type);
#endif
    }

    if (attribute_specifiers) {
        IntroAttributeData * d;
        uint32_t count;
        for (int i=0; i < arrlen(attribute_specifiers); i++) {
            int member_index = attribute_specifiers[i].i;
            char * location = attribute_specifiers[i].location;
            int error = parse_attributes(buffer, location, result, member_index, &d, &count);
            if (error) return error;
            result->members[member_index].attributes = d;
            result->members[member_index].count_attributes = count;
        }
        arrfree(attribute_specifiers);
    }

    structs[struct_index] = result;
    last_struct_parsed_index = struct_index;

    return 0;
}

static IntroType *
parse_base_type(ParseContext * ctx, char ** o_s) {
    IntroType type = {0};
    char * type_name = NULL;

    struct { // TODO: not sure what to do with this
        bool is_nested;
    } result = {0};

    Token first;
    do {
        first = next_token(o_s);
    } while (first.type == TK_IDENTIFIER && is_ignored(&first));
    if (first.type != TK_IDENTIFIER) {
        // TODO(print error)
        return NULL;
    }

    Token error_tk = {0};
    error_tk.start = first.start;
    error_tk.type = TK_IDENTIFIER;

    bool is_struct = tk_equal(&first, "struct");
    bool is_union  = tk_equal(&first, "union");
    bool is_enum   = tk_equal(&first, "enum");
    if (is_struct || is_union || is_enum) {
        char * after_keyword = *o_s;
        int error;
        strputf(&type_name, "%.*s", first.length, first.start);
        if (is_struct || is_union) {
            *o_s = first.start;
            error = parse_struct(ctx, o_s);
        } else {
            error = parse_enum(buffer, o_s);
        }
        if (error == 2) {
            *o_s = after_keyword;
            Token tk = next_token(o_s);
            strputf(&type_name, " %.*s", tk.length, tk.start);
        } else if (error != 0) {
            return NULL;
        } else {
            result.is_nested = true; // TODO: does this matter?
            return arrlast(ctx->types);
        }
    } else {
        Token tk, ltk = first;
        if (tk_equal(&first, "unsigned")) {
            type.category = INTRO_UNSIGNED;
        } else if (tk_equal(&first, "signed")) {
            type.category = INTRO_SIGNED;
        }

        if (type.category != INTRO_UNKNOWN) {
            strputf(&type_name, "%.*s", first.length, first.start);
            tk = next_token(o_s);
        } else {
            tk = first;
        }

        char * known_type_name = NULL;
        bool can_be_followed_by_int = true;
        if (type.category == INTRO_UNKNOWN) {
            can_be_followed_by_int = false;
            strputf(&type_name, "%.*s", tk.length, tk.start);
        } else {
            if (tk_equal(&tk, "long")) {
                Token tk2 = next_token(o_s);
                if (tk_equal(&tk2, "long")) {
                    known_type_name = "long long";
                    tk = tk2;
                } else if (tk_equal(&tk2, "double")) {
                    parse_error(&tk2, "long double is not supported.");
                    return NULL;
                } else {
                    known_type_name = "long";
                }
                type.category |= 0x08;
            } else if (tk_equal(&tk, "short")) {
                known_type_name = "short";
                type.category |= 0x02;
            } else if (tk_equal(&tk, "char")) {
                known_type_name = "char";
                type.category |= 0x01;
            }
        }

        if (can_be_followed_by_int) {
            if (known_type_name) {
                if (arrlen(type_name) > 0) {
                    strputf(&type_name, " ");
                }
                strputf(&type_name, known_type_name);
                ltk = tk;
                tk = next_token(o_s);
            }
            if (tk_equal(&tk, "int")) {
                strputf(&type_name, " int");
                ltk = tk;
            }
            if (type.category & 0x0f == 0) {
                type.category |= 0x04;
            }
        }

        error_tk.length = ltk.start - first.start + ltk.length;
        *o_s = ltk.start + ltk.length;
    }

    strputnull(type_name);
    IntroType * t = shget(ctx->type_map, type_name);
    if (t) {
        arrfree(type_name);
        return t;
    } else {
        type.name = type_name;
        arrfree(type_name);
        return store_type(ctx, &type);
    }
}

static IntroType *
parse_declaration(ParseContext * ctx, char ** o_s, char ** o_name) {
}

int
parse_preprocessed_text(char * buffer, IntroInfo * o_info) {
    ParseContext ctx = malloc(sizeof(ParseContext));
    memset(ctx, 0, sizeof(*ctx));

    sh_new_arena(ctx->type_map);
    sh_new_arena(ctx->member_map);

    static const IntroStruct known_types [] = {
        {"void",     NULL, INTRO_UNKNOWN},
        {"uint8_t",  NULL, INTRO_U8 },
        {"uint16_t", NULL, INTRO_U16},
        {"uint32_t", NULL, INTRO_U32},
        {"uint64_t", NULL, INTRO_U64},
        {"int8_t",   NULL, INTRO_S8 },
        {"int16_t",  NULL, INTRO_S16},
        {"int32_t",  NULL, INTRO_S32},
        {"int64_t",  NULL, INTRO_S64},
    };
    for (int i=0; i < LENGTH(known_types); i++) {
        store_type(ctx, &known_types[i]);
    }

    char * s = buffer;

    while (1) {
        Token tk = next_token(&s);

        if (tk.type == TK_END) {
            break;
        } else if (tk.type == TK_IDENTIFIER) {
            int error;
            if (tk_equal(&tk, "struct") || tk_equal(&tk, "union")) {
                s = tk.start;
                error = parse_struct(ctx, &s);
            } else if (tk_equal(&tk, "enum")) {
                error = parse_enum(ctx, &s);
            } else if (tk_equal(&tk, "typedef")) {
                error = parse_typedef(ctx, &s);
            } else {
                // TODO(print error)
                error = -1;
            }

            if (error) {
                return error;
            }
        } else if (tk.type == TK_L_BRACE) {
            s = find_closing(tk.start);
            if (!s) {
                // TODO(print error)
                return -1;
            }
            s++;
        }
    }
}
