#include "lexer.c"

#define BUCKET_CAP (1<<16)
typedef struct {
    int current;
    int current_used;
    struct {
        void * data;
    } buckets [64];
} MemArena;

typedef struct ExprNode ExprNode;
struct ExprNode {
    ExprNode * left;
    ExprNode * right;
    int depth;
    enum {
        OP_INT = 0x00,

        OP_UNARY_ADD = 0x01,
        OP_UNARY_SUB,
        OP_BIT_NOT,
        OP_BOOL_NOT,

        OP_MUL = 0x10,
        OP_DIV,
        OP_MOD,

        OP_ADD = 0x20,
        OP_SUB,

        OP_SHIFT_LEFT = 0x30,
        OP_SHIFT_RIGHT,

        OP_LESS= 0x40,
        OP_LESS_OR_EQUAL,
        OP_GREATER,
        OP_GREATER_OR_EQUAL,

        OP_EQUAL = 0x50,
        OP_NOT_EQUAL,

        OP_BIT_AND  = 0x60,
        OP_BIT_XOR  = 0x70,
        OP_BIT_OR   = 0x80,
        OP_BOOL_AND = 0x90,
        OP_BOOL_OR  = 0xa0,
    } op;
    intmax_t value;
    Token tk;
};

static void *
arena_alloc(MemArena * arena, size_t amount) {
    if (arena->current_used + amount > BUCKET_CAP) {
        arena->buckets[++arena->current].data = calloc(1, BUCKET_CAP);
    }
    void * result = arena->buckets[arena->current].data + arena->current_used;
    arena->current_used += amount;
    return result;
}

static MemArena *
new_arena() {
    MemArena * arena = calloc(1, sizeof(MemArena));
    arena->buckets[0].data = calloc(1, BUCKET_CAP);
    return arena;
}

static void
free_arena(MemArena * arena) {
    for (int i=0; i <= arena->current; i++) {
        free(arena->buckets[i].data);
    }
    free(arena);
}

ExprNode *
build_expression_tree(MemArena * arena, Token * tokens, int count_tokens) {
    ExprNode * base = NULL;

    int paren_depth = 0;
    ExprNode * node = arena_alloc(arena, sizeof(*node));
    for (int tk_i=0; tk_i < count_tokens; tk_i++) {
        Token tk = tokens[tk_i];

        switch(tk.type) {
        case TK_L_PARENTHESIS: {
            paren_depth += 1;
            continue;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            continue;
        }break;

        case TK_IDENTIFIER: {
            node->op = OP_INT;
            node->value = (intmax_t)strtol(tk.start, NULL, 0);
        }break;

        case TK_PLUS:     node->op = OP_ADD; break;
        case TK_HYPHEN:   node->op = OP_SUB; break;
        case TK_STAR:     node->op = OP_MUL; break;
        case TK_FORSLASH: node->op = OP_DIV; break;
        case TK_MOD:      node->op = OP_MOD; break;
        case TK_D_AND:    node->op = OP_BOOL_AND; break;
        case TK_D_BAR:    node->op = OP_BOOL_OR; break;

        default: {
            fprintf(stderr, "invalid symbol in expression: '%.*s'\n", tk.length, tk.start); // TODO
            exit(1);
        }break;
        }
        node->depth = paren_depth;
        node->tk = tk;

        ExprNode ** p_index = &base;
        while (1) {
            ExprNode * index = *p_index;
            if (
                (index == NULL)
             || (node->depth < index->depth)
             || (node->depth == index->depth && (node->op & 0xf0) >= (index->op & 0xf0))
               )
            {
                node->left = index;
                *p_index = node;
                break;
            }
            p_index = &index->right;
        }
        node = arena_alloc(arena, sizeof(*node));
    }

    return base;
}

void
expr_test() {
    char * buf = "7 / ((7 && 2) * 5) + 3 - (2*(3+4) - 2)";
    char * s = buf;
    Token * tks = NULL;
    while (1) {
        Token tk = next_token(&s);
        if (tk.type == TK_END) {
            break;
        }
        arrput(tks, tk);
    }

    MemArena * tree_arena = new_arena();
    ExprNode * tree = build_expression_tree(tree_arena, tks, arrlen(tks));

    free_arena(tree_arena);
}
