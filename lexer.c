#include "basic.h"
#include <ctype.h>

typedef struct Token {
    enum {
        TK_UNKNOWN,
        TK_IDENTIFIER,
        TK_STRING,
        TK_PARANTHESIS,
        TK_BRACKET,
        TK_BRACE,
        TK_EQUAL,
        TK_COLON,
        TK_SEMICOLON,
        TK_STAR,
        TK_PERIOD,
        TK_HASH,
        TK_END,
        TK_COUNT,
    } type;
    char * start;
    int length;
    bool is_open;
} Token;

Token
next_token(char ** o_s) {
    Token tk = {0};
    char * s = *o_s;
    while (1) {
        while (*s != '\0' && isspace(*s)) s++; 
        if (*s == '/' && *(s+1) != '\0') {
            if (*(s+1) == '/') {
                while (*++s != '\0' && *s != '\n' && *s != '\r');
            } else if (*++s == '*') {
                while (*++s != '\0' && !(*s == '/' && *(s-1) == '*'));
            }
        } else {
            break;
        }
    }
    if (*s == '\0') {
        tk.type = TK_END;
        return tk;
    }

    tk.start = s;

    if (isalnum(*s) || *s == '_') {
        tk.type = TK_IDENTIFIER;
        while (*++s != '\0' && (isalnum(*s) || *s == '_'));
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
        if (*s == '\0') {
            tk.type = TK_END;
            return tk;
        }
    }

    tk.length = 1;
    switch(*s) {
    case '{':
        tk.is_open = true; // FALLTHROUGH
    case '}':
        tk.type = TK_BRACE;
        break;

    case '[':
        tk.is_open = true; // FALLTHROUGH
    case ']':
        tk.type = TK_BRACKET;
        break;

    case '(':
        tk.is_open = true; // FALLTHROUGH
    case ')':
        tk.type = TK_PARANTHESIS;
        break;

    case '=': tk.type = TK_EQUAL; break;
    case ':': tk.type = TK_COLON; break;
    case ';': tk.type = TK_SEMICOLON; break;
    case '*': tk.type = TK_STAR; break;
    case '.': tk.type = TK_PERIOD; break;
    case '#': tk.type = TK_HASH; break;

    case EOF: tk.type = TK_END; break;

    default: tk.type = TK_UNKNOWN; break;
    }

    *o_s = s + 1;
    return tk;
}
