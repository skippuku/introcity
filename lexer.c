#ifndef LEXER_C
#define LEXER_C

#include <stdbool.h>
#include <stdio.h> // EOF
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Token {
    char * start;
    int16_t length;
    bool preceding_space;
    enum {
        TK_UNKNOWN,
        TK_L_PARENTHESIS,
        TK_R_PARENTHESIS,
        TK_L_BRACKET,
        TK_R_BRACKET,
        TK_L_BRACE,
        TK_R_BRACE,
        TK_L_ANGLE,
        TK_R_ANGLE,
        TK_EQUAL,
        TK_D_EQUAL,
        TK_COLON,
        TK_SEMICOLON,
        TK_STAR,
        TK_COMMA,
        TK_PERIOD,
        TK_HASH,
        TK_HYPHEN,
        TK_FORSLASH,
        TK_BACKSLASH,
        TK_BAR,
        TK_D_BAR,
        TK_AND,
        TK_D_AND,
        TK_PLUS,
        TK_CARET,
        TK_BANG,
        TK_MOD,
        TK_TILDE,

        TK_IDENTIFIER,
        TK_STRING,
        TK_COMMENT, // preprocessor only
        TK_NEWLINE, // preprocessor only
        TK_DISABLED, // preprocessor only
        TK_END,

        TK_COUNT
    } type;
} Token;

static bool
is_space(char c) {
    return c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == ' ';
}

static bool
is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool
is_iden(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool // TODO(remove) i don't thing this is useful actually
ignore_newline_at(char * s) {
    while (*--s != '\n' && is_space(*s));
    return *s == '\\';
}

Token
pre_next_token(char ** o_s) {
    Token tk = {0};
    tk.type = TK_END;

    char * s = *o_s;

    while (*s != '\0' && *s != '\n' && is_space(*s)) s++;
    tk.start = s;
    if (s != *o_s) tk.preceding_space = true;

    if (*s == '\0') return tk;

    if (*s == '\n') {
        tk.type = TK_NEWLINE;
        tk.length = 1;
        *o_s = s + 1;
        return tk;
    }

    if (*s == '\\') {
        while (1) {
            s++;
            if (*s == '\n') {
                tk.type = TK_COMMENT;
                tk.length = ++s - tk.start;
                *o_s = s;
                return tk;
            } else if (!is_space(*s)) {
                break;
            } else if (*s == '\0') {
                return tk;
            }
        }
    }

    if (*s == '/') {
        bool is_comment = false;
        if (*(s+1) == '/') {
            is_comment = true;
            while (*++s != '\0') {
                if (*s == '\n' && !ignore_newline_at(s)) {
                    break;
                }
            }
        } else if (*(s+1) == '*') {
            is_comment = true;
            while (*++s != '\0' && !(*s == '/' && *(s-1) == '*'));
            s++;
        }
        if (is_comment) {
            if (*s == '\0') return tk;

            tk.start = *o_s; // ignore preceeding whitespace
            tk.length = s - tk.start;
            tk.type = TK_COMMENT;
            *o_s = s;
            return tk;
        }
    }

    if (is_iden(*s)) {
        while (*++s != '\0' && is_iden(*s));
        tk.type = TK_IDENTIFIER;
        tk.length = s - tk.start;
        *o_s = s;
        return tk;
    }

    if (*s == '\'' || *s == '"') {
        char started_with = *s;
        while (*++s != '\0') {
            if (*s == started_with && !(*(s-1) == '\\' && *(s-2) != '\\')) {
                tk.type = TK_STRING;
                tk.length = ++s - tk.start;
                *o_s = s;
                return tk;
            }
        }
        if (*s == '\0') return tk;
    }

    tk.length = 1;

    // we'll just check what *s is since it's just one character
    // NOTE: maybe this should be how the parser version works too?
    if (*s != EOF) tk.type = TK_UNKNOWN;

    *o_s = s + 1;
    return tk;
}

Token
next_token(char ** o_s) {
    Token tk = {0};
    tk.type = TK_END;

    char * s = *o_s;
    while (*s != '\0' && is_space(*s)) s++;
    if (*s == '\0') return tk;

    tk.start = s;

    if (is_iden(*s)) {
        while (*++s != '\0' && is_iden(*s));
        tk.type = TK_IDENTIFIER;
        tk.length = s - tk.start;
        *o_s = s;
        return tk;
    }

    if (*s == '\'' || *s == '"') {
        char started_with = *s;
        while (*++s != '\0') {
            if (*s == started_with && !(*(s-1) == '\\' && *(s-2) != '\\')) {
                tk.type = TK_STRING;
                tk.length = ++s - tk.start;
                *o_s = s;
                return tk;
            }
        }
        if (*s == '\0') return tk;
    }

    tk.length = 1;
    switch(*s) {
    case '{': tk.type = TK_L_BRACE; break;
    case '}': tk.type = TK_R_BRACE; break;

    case '[': tk.type = TK_L_BRACKET; break;
    case ']': tk.type = TK_R_BRACKET; break;

    case '(': tk.type = TK_L_PARENTHESIS; break;
    case ')': tk.type = TK_R_PARENTHESIS; break;

    case '<': tk.type = TK_L_ANGLE; break;
    case '>': tk.type = TK_R_ANGLE; break;

    case ':': tk.type = TK_COLON; break;
    case ';': tk.type = TK_SEMICOLON; break;
    case '*': tk.type = TK_STAR; break;
    case ',': tk.type = TK_COMMA; break;
    case '.': tk.type = TK_PERIOD; break;
    case '#': tk.type = TK_HASH; break;
    case '-': tk.type = TK_HYPHEN; break;
    case '+': tk.type = TK_PLUS; break;
    case '^': tk.type = TK_CARET; break;
    case '/': tk.type = TK_FORSLASH; break;
    case '!': tk.type = TK_BANG; break;
    case '\\': tk.type = TK_BACKSLASH; break;
    case '~': tk.type = TK_TILDE; break;

    case '=': tk.type = TK_EQUAL; goto upgrade;
    case '|': tk.type = TK_BAR; goto upgrade;
    case '&': tk.type = TK_AND; goto upgrade;

    case EOF: tk.type = TK_END; break;

    default: tk.type = TK_UNKNOWN; break;

    upgrade: {
        if (*(s+1) == *s) {
            tk.type += 1;
            s += 1;
        }
    }break;
    }

    *o_s = s + 1;
    return tk;
}

char *
find_closing(char * s) {
    int depth = 1;
    char o = *s, c;
    switch(o) {
    case '{': c = '}'; break;
    case '[': c = ']'; break;
    case '(': c = ')'; break;
    case '<': c = '>'; break;
    default: return NULL;
    }
    s++;
    Token tk;
    while ((tk = next_token(&s)).type != TK_END) {
        if (tk.length == 1) {
            if (*tk.start == o) {
                depth++;
            } else if (*tk.start == c) {
                if (--depth == 0) return tk.start;
            }
        }
    }
    return NULL;
}

bool
tk_equal(Token * tk, const char * str) {
    size_t len = strlen(str);
    return (tk->length == len && memcmp(tk->start, str, len) == 0);
}

char *
copy_and_terminate(char * str, int length) {
    char * result = malloc(length + 1);
    memcpy(result, str, length);
    result[length] = '\0';
    return result;
}
#endif
