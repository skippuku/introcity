#ifndef LEXER_C
#define LEXER_C

#include <stdbool.h>
#include <stdio.h> // EOF
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TK_UNKNOWN,
    TK_L_PARENTHESIS,
    TK_R_PARENTHESIS,
    TK_L_BRACKET,
    TK_R_BRACKET,
    TK_L_BRACE,
    TK_R_BRACE,
    TK_L_ANGLE,
    TK_LEFT_SHIFT = TK_L_ANGLE + 1,
    TK_LESS_EQUAL = TK_L_ANGLE + 2,
    TK_R_ANGLE,
    TK_RIGHT_SHIFT = TK_R_ANGLE + 1,
    TK_GREATER_EQUAL = TK_R_ANGLE + 2,
    TK_EQUAL,
    TK_D_EQUAL = TK_EQUAL + 1,
    TK_COLON,
    TK_SEMICOLON,
    TK_STAR,
    TK_COMMA,
    TK_PERIOD,
    TK_HASH,
    TK_D_HASH = TK_HASH + 1,
    TK_HYPHEN,
    TK_FORSLASH,
    TK_BACKSLASH,
    TK_BAR,
    TK_D_BAR = TK_BAR + 1,
    TK_AND,
    TK_D_AND = TK_AND + 1,
    TK_PLUS,
    TK_CARET,
    TK_BANG,
    TK_NOT_EQUAL = TK_BANG + 2,
    TK_MOD,
    TK_TILDE,
    TK_QUESTION_MARK,
    TK_AT,

    TK_IDENTIFIER,
    TK_STRING,
    TK_NUMBER,

    TK_L_ARROW,
    TK_R_ARROW,

    // preprocessor only
    TK_COMMENT,
    TK_NEWLINE,
    TK_DISABLED,
    TK_PLACEHOLDER,
    TK_END,

    TK_COUNT
} TokenCode;

typedef struct Token {
    char * start;
    TokenCode type;
    int32_t index;
    int16_t length;
    bool preceding_space;
} Token;

typedef struct {
    Token * list;
    int32_t index;
} TokenIndex;

static bool
is_space(char c) {
    return c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == ' ';
}

static bool
is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool
is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool
is_iden(char c) {
    return is_digit(c) || is_alpha(c) || c == '_';
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

    if (is_iden(*s)) {
        if (is_digit(*s)) {
            bool is_integer = true;
            if (*s == '0') {
                switch (*(s+1)) {
                case 'x': s++;
                          do ++s; while (is_digit(*s) || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'));
                          break;
                case 'b': s++;
                          do ++s; while (*s == '0' || *s == '1');
                          break;
                case '.': ++s; break;
                default:  do ++s; while (*s >= '0' && *s <= '7');
                          break;
                }
            } else {
                while (is_digit(*++s));
            }
            if (*s == '.') {
                is_integer = false;
                while (is_digit(*++s));
            }
            if (*s == 'f' || *s == 'F') {
                is_integer = false;
                ++s;
            }

            if (is_integer) {
                while (*s == 'l' || *s == 'L' || *s == 'u' || *s == 'U') ++s;
            }
            tk.type = TK_NUMBER;
        } else {
            while (is_iden(*++s));
            tk.type = TK_IDENTIFIER;
        }
        tk.length = s - tk.start;
        *o_s = s;
        return tk;
    }

    enum TokenFlags {
        TK_CHECK_DOUBLE = 0x01,
        TK_CHECK_EQUAL  = 0x02,
    } flags = 0;

    tk.length = 1;
    switch(*s) {
    case '\0': return tk;

    case '\n': {
        tk.type = TK_NEWLINE;
        tk.length = 1;
        *o_s = s + 1;
        return tk;
    }

    case '#': {
        if (*(s+1) == '#') {
            tk.length = 2;
            tk.type = TK_D_HASH;
            *o_s = s + 2;
        } else {
            tk.length = 1;
            tk.type = TK_HASH;
            *o_s = s + 1;
        }
        return tk;
    }

    case '\\': {
        while (1) {
            s++;
            if (*s == '\n') {
                tk.type = TK_COMMENT;
                tk.length = ++s - tk.start;
                *o_s = s;
                return tk;
            } else if (!is_space(*s)) {
                tk.type = TK_BACKSLASH;
                s = *o_s;
                goto end;
            } else if (*s == '\0') {
                return tk;
            }
        }
    }

    case '\'': case '"': {
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

    case '/': {
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
        } else {
            tk.type = TK_FORSLASH;
            break;
        }
    }

    case EOF: {
        *o_s = s;
        return tk;
    }

    case '{': tk.type = TK_L_BRACE; break;
    case '}': tk.type = TK_R_BRACE; break;

    case '[': tk.type = TK_L_BRACKET; break;
    case ']': tk.type = TK_R_BRACKET; break;

    case '(': tk.type = TK_L_PARENTHESIS; break;
    case ')': tk.type = TK_R_PARENTHESIS; break;

    case '<': if (*(s+1) == '-') {
                  tk.length = 2;
                  tk.type = TK_L_ARROW;
                  *o_s = s + 2;
                  return tk;
              }
              tk.type = TK_L_ANGLE;
              flags = TK_CHECK_DOUBLE | TK_CHECK_EQUAL;
              break;
    case '>': tk.type = TK_R_ANGLE;
              flags = TK_CHECK_DOUBLE | TK_CHECK_EQUAL; break;

    case ':': tk.type = TK_COLON; break;
    case ';': tk.type = TK_SEMICOLON; break;
    case '*': tk.type = TK_STAR; break;
    case ',': tk.type = TK_COMMA; break;
    case '.': tk.type = TK_PERIOD; break;
    case '-': if (*(s+1) == '>') {
                  tk.length = 2;
                  tk.type = TK_R_ARROW;
                  *o_s = s + 2;
                  return tk;
              }
              tk.type = TK_HYPHEN;
              break;
    case '+': tk.type = TK_PLUS; break;
    case '^': tk.type = TK_CARET; break;
    case '~': tk.type = TK_TILDE; break;
    case '%': tk.type = TK_MOD; break;
    case '?': tk.type = TK_QUESTION_MARK; break;
    case '@': tk.type = TK_AT; break;
    case '!': tk.type = TK_BANG;
              flags = TK_CHECK_EQUAL; break;

    case '=': tk.type = TK_EQUAL;
              flags = TK_CHECK_DOUBLE; break;
    case '|': tk.type = TK_BAR;
              flags = TK_CHECK_DOUBLE; break;
    case '&': tk.type = TK_AND;
              flags = TK_CHECK_DOUBLE; break;

    default: tk.type = TK_UNKNOWN; break;
    }

    if ((flags & TK_CHECK_DOUBLE)) {
        if (*(s+1) == *s) {
            tk.type += 1;
            tk.length += 1;
            s += 1;
            flags = 0;
        }
    }
    if ((flags & TK_CHECK_EQUAL)) {
        if (*(s+1) == '=') {
            tk.type += 2;
            tk.length += 1;
            s += 1;
        }
    }

end:
    *o_s = s + 1;
    return tk;
}

static inline Token
tk_at(const TokenIndex * tidx) {
    return tidx->list[tidx->index];
}

static inline Token
next_token(TokenIndex * tidx) {
    Token tk = tidx->list[tidx->index];
    tidx->index++;
    return tk;
}

static void
advance_to(TokenIndex * tidx, char * location) {
    Token tk;
    while (1) {
        tk = next_token(tidx);
        if (location <= tk.start) {
            tidx->index--;
            return;
        }
    }
}

Token *
create_token_list(char * buffer) {
    char * s = buffer;

    Token * list = NULL;
    arrsetcap(list, 64);

    while (1) {
        Token tk = pre_next_token(&s);
        arrput(list, tk);
        if (tk.type == TK_END) {
            break;
        }
    }

    return list;
}

int32_t
find_closing(TokenIndex idx) {
    int depth = 1;
    char o = *tk_at(&idx).start, c;
    switch(o) {
    case '{': c = '}'; break;
    case '[': c = ']'; break;
    case '(': c = ')'; break;
    case '<': c = '>'; break;
    default: return 0;
    }
    idx.index++;
    Token tk;
    while ((tk = next_token(&idx)).type != TK_END) {
        if (*tk.start == o) {
            depth++;
        } else if (*tk.start == c) {
            if (--depth == 0) return idx.index;
        }
    }
    return 0;
}

static bool
tk_equal(const Token * tk, const char * str) {
    for (int i=0; i < tk->length; i++) {
        if (tk->start[i] != str[i]) {
            return false;
        }
    }
    if (str[tk->length] == '\0') return true;
    return false;
}
#endif
