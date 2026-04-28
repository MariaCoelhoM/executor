#include "ndr.h"

/**
 * Cria um nó do tipo NUMÉRICO.
 * Representa um valor constante (folha da árvore).
 */
ast_node_t *ast_num(long value) {
    // Aloca memória zerada para o nó
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_NUM;   // Define o tipo como número
    n->value = value;     // Armazena o valor numérico
    return n;
}

/**
 * Cria um nó do tipo OPERAÇÃO BINÁRIA (+, -, *, etc).
 * Requer dois operandos: esquerda (l) e direita (r).
 */
ast_node_t *ast_binop(char op, ast_node_t *l, ast_node_t *r) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_BINOP; // Define o tipo como operação binária
    n->value = (long)op;  // Armazena o caractere do operador como long
    n->left  = l;         // Aponta para o sub-nó esquerdo
    n->right = r;         // Aponta para o sub-nó direito
    return n;
}

/**
 * Cria um nó do tipo OPERAÇÃO UNÁRIA (ex: sinal de negativo -x).
 * Requer apenas um operando.
 */
ast_node_t *ast_unary(char op, ast_node_t *operand) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->kind  = AST_UNARY; // Define o tipo como operação unária
    n->value = (long)op;  // Armazena o operador
    n->left  = operand;   // O operando fica sempre à esquerda
    return n;
}

/**
 * Desaloca recursivamente a memória de toda a árvore.
 * Previne vazamentos de memória (memory leaks).
 */
void ast_free(ast_node_t *n) {
    if (!n) return;       // Caso base: nó nulo
    ast_free(n->left);    // Libera sub-árvore esquerda
    ast_free(n->right);   // Libera sub-árvore direita
    free(n);              // Libera o nó atual
}

/**
 * Imprime a estrutura da árvore no terminal com indentação.
 * Útil para visualizar a hierarquia da expressão.
 */
void ast_print(ast_node_t *n, int depth) {
    if (!n) return;
    
    // Cria o recuo visual baseado na profundidade (depth)
    for (int i = 0; i < depth * 3; i++) putchar(' ');
    
    // Identifica o tipo do nó e imprime o conteúdo
    switch (n->kind) {
        case AST_NUM:   printf("NUM(%ld)\n", n->value);               break;
        case AST_BINOP: printf("BINOP('%c')\n", (char)n->value);      break;
        case AST_UNARY: printf("UNARY('%c')\n", (char)n->value);      break;
    }
    
    // Chama recursivamente para os filhos aumentando a profundidade
    ast_print(n->left,  depth + 1);
    ast_print(n->right, depth + 1);
}

/**
 * Avalia (resolve) a expressão matemática da árvore.
 * Retorna o resultado final e usa o ponteiro 'ok' para sinalizar erros.
 */
long ast_eval(ast_node_t *n, int *ok) {
    if (!n) return 0;
    
    // Se for um número, retorna o próprio valor (fim da recursão)
    if (n->kind == AST_NUM) return n->value;

    // Resolve recursivamente o lado esquerdo
    long l = ast_eval(n->left,  ok);
    
    // Se for binário, resolve o lado direito; se for unário, o lado direito é 0
    long r = (n->kind == AST_BINOP) ? ast_eval(n->right, ok) : 0;

    // Lógica para operações unárias (ex: inverter sinal)
    if (n->kind == AST_UNARY) {
        return (n->value == '-') ? -l : l;
    }

    // Lógica para operações binárias
    char op = (char)n->value;
    switch (op) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/':
            // Proteção contra divisão por zero
            if (r == 0) { 
                fprintf(stderr, "[AST] Erro: divisao por zero\n"); 
                *ok = 0; 
                return 0; 
            }
            return l / r;
        case '%':
            // Proteção contra módulo por zero
            if (r == 0) { 
                fprintf(stderr, "[AST] Erro: modulo por zero\n");  
                *ok = 0; 
                return 0; 
            }
            return l % r;
    }
    return 0;
}