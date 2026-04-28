#include "ndr.h"

ast_node_t *ast_num(long value) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_NUM;
    n->value = value;
    return n;
}

ast_node_t *ast_binop(char op, ast_node_t *l, ast_node_t *r) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_BINOP;
    n->value = (long)op;
    n->left  = l;
    n->right = r;
    return n;
}

ast_node_t *ast_unary(char op, ast_node_t *operand) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_UNARY;
    n->value = (long)op;
    n->left  = operand;
    return n;
}

void ast_free(ast_node_t *n) {
    if (!n) return;
    ast_free(n->left);
    ast_free(n->right);
    free(n);
}

void ast_print(ast_node_t *n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth * 3; i++) putchar(' ');
    switch (n->kind) {
        case AST_NUM:   printf("NUM(%ld)\n", n->value);               break;
        case AST_BINOP: printf("BINOP('%c')\n", (char)n->value);      break;
        case AST_UNARY: printf("UNARY('%c')\n", (char)n->value);      break;
    }
    ast_print(n->left,  depth + 1);
    ast_print(n->right, depth + 1);
}

long ast_eval(ast_node_t *n, int *ok) {
    if (!n) return 0;
    if (n->kind == AST_NUM) return n->value;

    long l = ast_eval(n->left,  ok);
    long r = (n->kind == AST_BINOP) ? ast_eval(n->right, ok) : 0;

    if (n->kind == AST_UNARY) {
        return (n->value == '-') ? -l : l;
    }

    char op = (char)n->value;
    switch (op) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/':
            if (r == 0) { fprintf(stderr, "[AST] Erro: divisao por zero\n"); *ok = 0; return 0; }
            return l / r;
        case '%':
            if (r == 0) { fprintf(stderr, "[AST] Erro: modulo por zero\n");  *ok = 0; return 0; }
            return l % r;
    }
    return 0;
}
