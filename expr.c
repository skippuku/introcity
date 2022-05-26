#include "lexer.c"
#include "global.h"

#define EXPR_BUCKET_CAP (1<<12)

typedef enum {
    OP_INT = 0x00,

    OP_UNARY_ADD = 0x21,
    OP_UNARY_SUB,
    OP_BIT_NOT,
    OP_BOOL_NOT,

    OP_MUL = 0x30,
    OP_DIV,
    OP_MOD,

    OP_ADD = 0x40,
    OP_SUB,

    OP_SHIFT_LEFT = 0x50,
    OP_SHIFT_RIGHT,

    OP_LESS = 0x60,
    OP_LESS_OR_EQUAL,
    OP_GREATER,
    OP_GREATER_OR_EQUAL,

    OP_EQUAL = 0x70,
    OP_NOT_EQUAL,

    OP_BIT_AND  = 0x80,
    OP_BIT_XOR  = 0x90,
    OP_BIT_OR   = 0xa0,
    OP_BOOL_AND = 0xb0,
    OP_BOOL_OR  = 0xc0,

    OP_TERNARY_1 = 0xd0,
    OP_TERNARY_2,

    OP_PUSH = 0xf0, // intruction only
    OP_SET,
    OP_DONE,
} ExprOp;

enum ExprOpTypes {
    OP_TYPE_MASK  = 0xf0,
    OP_VALUE_TYPE = 0x00,
    OP_UNARY_TYPE = 0x20,
};

struct ExprContext {
    struct{char * key; intmax_t value;} * constant_map;
    MemArena * arena;
    ParseContext * ctx;
    enum {
        MODE_PRE,
        MODE_PARSE,
    } mode;
};

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
    REG_VALUE,
    REG_LAST_RESULT,
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

static void
free_expr_context(ExprContext * ectx) {
    free_arena(ectx->arena);
    shfree(ectx->constant_map);
}

ExprNode *
build_expression_tree(ExprContext * ectx, Token * tokens, int count_tokens, Token * o_error_tk) {
    ExprNode * base = NULL;

    int paren_depth = 0;
    ExprNode * node = arena_alloc(ectx->arena, sizeof(*node));
    bool last_was_value = false;
    for (int tk_i=0; tk_i < count_tokens; tk_i++) {
        Token tk = tokens[tk_i];

        switch(tk.type) {
        case TK_END: {
            tk_i = count_tokens; // break loop
            continue;
        }break;

        case TK_L_PARENTHESIS: {
            paren_depth += 1;
            continue;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            continue;
        }break;

        case TK_IDENTIFIER: {
            if (ectx->mode == MODE_PARSE && !is_digit(tk.start[0])) {
                if (tk_equal(&tk, "sizeof")) {
                    if (tk_i + 3 >= count_tokens) {
                        *o_error_tk = tk;
                        return NULL;
                    }
                    Token tk1 = tokens[++tk_i];
                    if (tk1.type != TK_L_PARENTHESIS) {
                        *o_error_tk = tk1;
                        return NULL;
                    }

                    tk1 = tokens[++tk_i];
                    char * s = tk1.start;
                    DeclState cast = {.state = DECL_CAST};
                    int ret = parse_declaration(ectx->ctx, &s, &cast);
                    if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                        node->value = intro_size(cast.type);
                    } else if (ret == RET_NOT_TYPE) {
                        if (tk1.type == TK_STRING) {
                            node->value = tk1.length - 1;
                        } else {
                            return NULL;
                        }
                    } else {
                        return NULL;
                    }

                    node->op = OP_INT;

                    while (tk1.start < s && ++tk_i < count_tokens) tk1 = tokens[tk_i];
                    tk_i--;
                    break;
                }
                STACK_TERMINATE(terminated, tk.start, tk.length);
                ptrdiff_t const_index = shgeti(ectx->constant_map, terminated);
                // NOTE: dumb hack to get around casts... if they are to int
                if (paren_depth > 0 && tk_equal(&tk, "int")) {
                    while (1) {
                        if (++tk_i >= count_tokens) {
                            *o_error_tk = tk;
                            return NULL;
                        }
                        Token tk1 = tokens[tk_i];
                        if (tk1.type == TK_R_PARENTHESIS) {
                            paren_depth -= 1;
                            break;
                        }
                    }
                    continue;
                }
                if (const_index < 0) {
                    *o_error_tk = tk;
                    return NULL;
                }
                node->value = ectx->constant_map[const_index].value;
            } else {
                node->value = (intmax_t)strtol(tk.start, NULL, 0);
            }
            node->op = OP_INT;
        }break;

        case TK_PLUS:          node->op = (last_was_value)? OP_ADD : OP_UNARY_ADD; break;
        case TK_HYPHEN:        node->op = (last_was_value)? OP_SUB : OP_UNARY_SUB; break;
        case TK_STAR:          node->op = OP_MUL; break;
        case TK_FORSLASH:      node->op = OP_DIV; break;
        case TK_MOD:           node->op = OP_MOD; break;
        case TK_D_EQUAL:       node->op = OP_EQUAL; break;
        case TK_NOT_EQUAL:     node->op = OP_NOT_EQUAL; break;
        case TK_D_AND:         node->op = OP_BOOL_AND; break;
        case TK_D_BAR:         node->op = OP_BOOL_OR; break;
        case TK_L_ANGLE:       node->op = OP_LESS; break;
        case TK_LESS_EQUAL:    node->op = OP_LESS_OR_EQUAL; break;
        case TK_R_ANGLE:       node->op = OP_GREATER; break;
        case TK_GREATER_EQUAL: node->op = OP_GREATER_OR_EQUAL; break;
        case TK_BAR:           node->op = OP_BIT_OR; break;
        case TK_AND:           node->op = OP_BIT_AND; break;
        case TK_CARET:         node->op = OP_BIT_XOR; break;
        case TK_BANG:          node->op = OP_BOOL_NOT; break;
        case TK_TILDE:         node->op = OP_BIT_NOT; break;
        case TK_LEFT_SHIFT:    node->op = OP_SHIFT_LEFT; break;
        case TK_RIGHT_SHIFT:   node->op = OP_SHIFT_RIGHT; break;
        case TK_QUESTION_MARK: node->op = OP_TERNARY_1; break;
        case TK_COLON:         node->op = OP_TERNARY_2; break;

        default: {
            if (o_error_tk) *o_error_tk = tk;
            return NULL;
        }break;
        }

        if (node->op == OP_TERNARY_2) paren_depth -= 1;

        node->depth = paren_depth;
        node->tk = tk;

        if (node->op == OP_TERNARY_1) paren_depth += 1;

        last_was_value = (node->op == OP_INT);

        ExprNode ** p_index = &base;
        while (1) {
            ExprNode * index = *p_index;

            if (
                (index == NULL)
              ||(node->depth < index->depth)
              ||(
                 (node->depth == index->depth && (node->op & OP_TYPE_MASK) >= (index->op & OP_TYPE_MASK))
               &&((node->op & OP_TYPE_MASK) != OP_UNARY_TYPE)
                )
               )
            {
                node->left = index;
                *p_index = node;
                break;
            }
            p_index = &index->right;
        }
        node = arena_alloc(ectx->arena, sizeof(*node));
    }

    return base;
}

ExprProcedure *
build_expression_procedure(ExprNode * tree) {
    ExprInstruction * list = NULL;

    ExprNode ** node_stack = NULL;
    ExprNode * node = tree;
    int max_stack_size = 0;

    if (!node || node->op == OP_INT) {
        ExprInstruction set = {
            .op = OP_SET,
            .left_type = REG_VALUE,
            .right_type = REG_VALUE,
            .right_value = (node)? node->value : 0,
        };
        arrput(list, set);
        goto post_reverse;
    }

    while (1) {
        ExprInstruction ins = {0};
        ins.op = node->op;
        bool left_is_op = node->left && node->left->op;
        bool right_is_op = node->right->op;
        bool go_left = false;
        bool take_from_node_stack = false;
        if (node->op == OP_TERNARY_1) {
            (void)arrpop(node_stack);
        }
        if (node->op == OP_TERNARY_2) {
            arrput(node_stack, NULL); // dummy node to get correct stack size
        }
        if (left_is_op) {
            if (right_is_op) {
                ins.left_type = REG_POP_STACK;
                ins.right_type = REG_LAST_RESULT;
                arrput(node_stack, node->left);
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
        if (arrlen(node_stack) > max_stack_size) {
            max_stack_size = arrlen(node_stack);
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

post_reverse: ;
    static const ExprInstruction done = {.op = OP_DONE};
    arrput(list, done);

    ExprProcedure * proc = malloc(sizeof(*proc) + sizeof(*list) * arrlen(list));
    proc->stack_size = max_stack_size,
    proc->count_instructions = arrlen(list),
    memcpy(proc->instructions, list, sizeof(*list) * arrlen(list));

    arrfree(list);

    return proc;
}

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
intmax_t
run_expression(ExprProcedure * proc) {
    intmax_t stack [proc->stack_size + 1]; // +1: no undefined behavior
    intmax_t stack_index = 0;
    intmax_t result = 0;
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
        case REG_POP_STACK: break; // never reached
        }

        switch(ins.op) {
        case OP_DONE: return result;
        case OP_PUSH: stack[stack_index++] = result; break;

        case OP_UNARY_ADD: result = +right; break;
        case OP_UNARY_SUB: result = -right; break;
        case OP_BOOL_NOT:  result = !right; break;
        case OP_BIT_NOT:   result = ~right; break;

        case OP_ADD: result = left + right; break;
        case OP_SUB: result = left - right; break;
        case OP_MUL: result = left * right; break;
        case OP_DIV: result = left / right; break;
        case OP_MOD: result = left % right; break;

        case OP_EQUAL:            result = left == right; break;
        case OP_NOT_EQUAL:        result = left != right; break;
        case OP_LESS:             result = left <  right; break;
        case OP_GREATER:          result = left >  right; break;
        case OP_LESS_OR_EQUAL:    result = left <= right; break;
        case OP_GREATER_OR_EQUAL: result = left >= right; break;
        case OP_BOOL_AND:         result = left && right; break;
        case OP_BOOL_OR:          result = left || right; break;
        case OP_SHIFT_LEFT:       result = left << right; break;
        case OP_SHIFT_RIGHT:      result = left >> right; break;
        
        case OP_BIT_AND: result = left & right; break;
        case OP_BIT_OR:  result = left | right; break;
        case OP_BIT_XOR: result = left ^ right; break;

        case OP_SET: result = right; break;

        case OP_TERNARY_1:
            stack[stack_index++] = !!left;
            result = right;
            break;

        case OP_TERNARY_2:
            result = (stack[--stack_index])? left : right;
            break;

        case OP_INT: break; // never reached
        }
    }

    return result;
}
#pragma GCC diagnostic pop

void
expr_test() {
    char buf [1024];
    printf("Enter a c expression: ");
    assert(fgets(buf, sizeof(buf), stdin));
    char * s = buf;
    Token * tks = NULL;
    while (1) {
        Token tk = next_token(&s);
        if (tk.type == TK_END) {
            break;
        }
        arrput(tks, tk);
    }

    ExprContext ectx = {
        .mode = MODE_PRE,
        .arena = new_arena(EXPR_BUCKET_CAP),
    };
    ExprNode * tree = build_expression_tree(&ectx, tks, arrlen(tks), NULL);
    ExprProcedure * expr = build_expression_procedure(tree);
    intmax_t result = run_expression(expr);

    free(expr);
    free_expr_context(&ectx);

    printf("result: %i\n", (int)result);
}
