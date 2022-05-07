#include "lexer.c"

#define BUCKET_CAP (1<<16)
typedef struct {
    int current;
    int current_used;
    struct {
        void * data;
    } buckets [64];
} MemArena;

typedef enum {
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

    OP_PUSH = 0xf0, // intruction only
    OP_DONE,        // intruction only
} ExprOp;

typedef struct ExprNode ExprNode;
struct ExprNode {
    ExprNode * left;
    ExprNode * right;
    int depth;
    ExprOp op;
    intmax_t value;
    Token tk;
};

typedef enum {
    REG_LAST_RESULT,
    REG_VALUE,
    REG_POP_STACK,
} ExprInstructionRegisterType;

typedef struct {
    ExprOp op;
    ExprInstructionRegisterType left_type, right_type;
    intmax_t left_value, right_value;
} ExprInstruction;

typedef struct {
    int stack_size;
    int count_instructions;
    ExprInstruction instructions [];
} ExprProcedure;

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

ExprProcedure *
build_expr_procedure(ExprNode * tree) {
    ExprInstruction * list = NULL;

    ExprNode ** node_stack = NULL;
    ExprNode * node = tree;
    int max_stack_size = 0;

    while (1) {
        ExprInstruction ins = {0};
        ins.op = node->op;
        bool left_is_op = node->left && node->left->op;
        bool right_is_op = node->right->op;
        bool go_left = false;
        bool take_from_node_stack = false;
        if (left_is_op) {
            if (right_is_op) {
                ins.left_type = REG_POP_STACK;
                ins.right_type = REG_LAST_RESULT;
                arrput(node_stack, node->left);
                if (arrlen(node_stack) > max_stack_size) {
                    max_stack_size = arrlen(node_stack);
                }
            } else {
                ins.left_type = REG_LAST_RESULT;
                ins.right_type = REG_VALUE;
                go_left = true;
            }
        } else {
            if (right_is_op) {
                ins.left_type = REG_VALUE;
                ins.right_type = REG_LAST_RESULT;
            } else {
                ins.left_type = REG_VALUE;
                ins.right_type = REG_VALUE;
                take_from_node_stack = true;
            }
        }

        if (ins.left_type == REG_VALUE) {
            if (node->left) {
                ins.left_value = node->left->value;
            } else {
                ins.left_value = 0;
            }
        }
        if (ins.right_type == REG_VALUE) {
            ins.right_value = node->right->value;
        }

        arrput(list, ins);

        if (take_from_node_stack) {
            if (arrlen(node_stack) > 0) {
                static const ExprInstruction push = {.op = OP_PUSH};
                arrput(list, push);
                node = arrpop(node_stack);
            } else {
                break;
            }
        } else {
            if (go_left) {
                node = node->left;
            } else {
                node = node->right;
            }
        }
    }

    // reverse list
    for (int i=0; i < arrlen(list) / 2; i++) {
        int opposite_index = arrlen(list) - i - 1;
        ExprInstruction temp = list[opposite_index];
        list[opposite_index] = list[i];
        list[i] = temp;
    }

    static const ExprInstruction done = {.op = OP_DONE};
    arrput(list, done);

    ExprProcedure * proc = malloc(sizeof(*proc) + sizeof(*list) * arrlen(list));
    proc->stack_size = max_stack_size,
    proc->count_instructions = arrlen(list),
    memcpy(proc->instructions, list, sizeof(*list) * arrlen(list));

    arrfree(list);

    return proc;
}

intmax_t
run_expression(ExprProcedure * proc) {
    intmax_t stack [proc->stack_size];
    intmax_t stack_index = 0;
    intmax_t result;
    for (int i=0; i < proc->count_instructions; i++) {
        ExprInstruction ins = proc->instructions[i];

        intmax_t left, right;
        switch(ins.left_type) {
        case REG_VALUE: left = ins.left_value; break;
        case REG_LAST_RESULT: left = result; break;
        case REG_POP_STACK: left = stack[--stack_index]; break;
        }

        switch(ins.right_type) {
        case REG_VALUE: right = ins.right_value; break;
        case REG_LAST_RESULT: right = result; break;
        default: ;
        }

        switch(ins.op) {
        case OP_DONE: return result;
        case OP_PUSH: stack[stack_index++] = result; break;

        case OP_ADD: result = left + right; break;
        case OP_SUB: result = left - right; break;
        case OP_MUL: result = left * right; break;
        case OP_DIV: result = left / right; break;
        case OP_MOD: result = left % right; break;
        case OP_BOOL_AND: result = left && right; break;
        case OP_BOOL_OR: result = left || right; break;

        default: ;
        }
    }

    return result;
}

void
expr_test() {
    char * buf = "7 / ((7 && 2) * 5) - 3 + (2*(3+4) - 2)";
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
    ExprProcedure * expr = build_expr_procedure(tree);
    intmax_t result = run_expression(expr);

    free(expr);
    free_arena(tree_arena);

    printf("expression: %s\n", buf);
    printf("result: %i\n", (int)result);
}
