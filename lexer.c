#include <ctype.h>
#include <stdbool.h>
#include <stdio.h> // EOF

typedef struct Token {
    char * start;
    int32_t length;
    enum {
        TK_UNKNOWN,
        TK_IDENTIFIER,
        TK_STRING,
        TK_L_PARENTHESIS,
        TK_R_PARENTHESIS,
        TK_L_BRACKET,
        TK_R_BRACKET,
        TK_L_BRACE,
        TK_R_BRACE,
        TK_EQUAL,
        TK_COLON,
        TK_SEMICOLON,
        TK_STAR,
        TK_COMMA,
        TK_PERIOD,
        TK_HASH,
        TK_HYPHEN,
        TK_END,
        TK_COUNT
    } type;
} Token;

Token
next_token(char ** o_s) {
    Token tk = {0};
    tk.type = TK_END;

    char * s = *o_s;
    while (1) {
        while (*s != '\0' && isspace(*s)) s++; 
        if (*s == '/') {
            if (*(s+1) == '/') {
                while (*++s != '\0' && *s != '\n');
            } else if (*++s == '*') {
                while (*++s != '\0' && !(*s == '/' && *(s-1) == '*'));
            }
        } else {
            break;
        }
    }
    if (*s == '\0') return tk;

    tk.start = s;

    if (isalnum(*s) || *s == '_') {
        while (*++s != '\0' && (isalnum(*s) || *s == '_'));
        tk.type = TK_IDENTIFIER;
        tk.length = s - tk.start;
        *o_s = s;
        return tk;
    }

    if (*s == '\'' || *s == '"') {
        tk.start++;
        char started_with = *s;
        while (*++s != '\0') {
            if (*s == started_with && *(s-1) != '\\') {
                tk.type = TK_STRING;
                tk.length = s - tk.start;
                *o_s = s + 1;
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

    case '=': tk.type = TK_EQUAL; break;
    case ':': tk.type = TK_COLON; break;
    case ';': tk.type = TK_SEMICOLON; break;
    case '*': tk.type = TK_STAR; break;
    case ',': tk.type = TK_COMMA; break;
    case '.': tk.type = TK_PERIOD; break;
    case '#': tk.type = TK_HASH; break;
    case '-': tk.type = TK_HYPHEN; break;

    case EOF: tk.type = TK_END; break;

    default: tk.type = TK_UNKNOWN; break;
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
    default: return NULL;
    }
    while (*++s != '\0') {
        if (*s == o) {
            depth++;
        } else if (*s == c) {
            if (--depth == 0) return s;
        }
    }
    return NULL;
}
