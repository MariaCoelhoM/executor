#include "ndr.h"

/*
 * Gramática (precedência crescente):
 * Aqui definimos a hierarquia das operações. O Parser começa de baixo
 * para cima na precedência para garantir que os operadores "mais fortes" 
 * fiquem mais profundos na árvore.
 *
 * expr      → additive
 * additive  → mult ( ('+' | '-') mult )*
 * mult      → unary ( ('*' | '/' | '%') unary )*
 * unary     → '-' unary | primary
 * primary   → NUMBER | '(' expr ')'
 */

// Protótipos das funções internas para permitir a recursão
static ast_node_t *parse_expr     (rparser_t *p);
static ast_node_t *parse_additive(rparser_t *p);
static ast_node_t *parse_mult    (rparser_t *p);
static ast_node_t *parse_unary   (rparser_t *p);
static ast_node_t *parse_primary (rparser_t *p);

/* Auxiliar: Captura o token atual do Lexer e avança para o próximo */
static tok_t consume(rparser_t *p) {
    tok_t t = p->lx->peek(p->lx);
    p->lx->next(p->lx);
    return t;
}

/* Auxiliar: Apenas verifica se o token atual é de um determinado tipo */
static int check(rparser_t *p, tok_kind_t kind) {
    return p->lx->peek(p->lx).kind == kind;
}

/**
 * PARSE PRIMARY: O nível mais básico.
 * Trata números ou expressões entre parênteses.
 */
static ast_node_t *parse_primary(rparser_t *p) {
    // Se for um número, cria um nó folha do tipo AST_NUM
    if (check(p, TOK_NUM)) {
        tok_t t = consume(p);
        return ast_num(t.value);
    }
    
    // Se for '(', inicia uma nova expressão recursivamente
    if (check(p, TOK_LPAREN)) {
        consume(p); /* consome '(' */
        ast_node_t *inner = parse_expr(p);
        
        // Verifica se o parêntese foi devidamente fechado
        if (!check(p, TOK_RPAREN)) {
            fprintf(stderr, "[PARSER] Erro: ')' esperado\n");
            p->ok = 0;
            ast_free(inner);
            return ast_num(0);
        }
        consume(p); /* consome ')' */
        return inner;
    }
    
    fprintf(stderr, "[PARSER] Erro: numero ou '(' esperado\n");
    p->ok = 0;
    return ast_num(0);
}

/**
 * PARSE UNARY: Trata operadores que agem sobre um único valor.
 * Exemplo: -5 ou +10.
 */
static ast_node_t *parse_unary(rparser_t *p) {
    if (check(p, TOK_MINUS)) {
        consume(p);
        // Recursão aqui permite coisas como --5
        return ast_unary('-', parse_unary(p));
    }
    if (check(p, TOK_PLUS)) {
        consume(p);
        return ast_unary('+', parse_unary(p));
    }
    return parse_primary(p);
}

/**
 * PARSE MULT: Trata multiplicação, divisão e resto.
 * Como têm maior precedência que a soma, são chamados depois dela.
 */
static ast_node_t *parse_mult(rparser_t *p) {
    ast_node_t *left = parse_unary(p);
    
    // Enquanto houver operadores de multiplicação/divisão, continua agrupando
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        tok_t op = consume(p);
        ast_node_t *right = parse_unary(p);
        // Cria um nó binário ligando o resultado anterior com o novo termo
        left = ast_binop((char)op.value, left, right);
    }
    return left;
}

/**
 * PARSE ADDITIVE: Trata soma e subtração.
 */
static ast_node_t *parse_additive(rparser_t *p) {
    ast_node_t *left = parse_mult(p);
    
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        tok_t op = consume(p);
        ast_node_t *right = parse_mult(p);
        left = ast_binop((char)op.value, left, right);
    }
    return left;
}

/**
 * Ponto de entrada da lógica de parsing.
 */
static ast_node_t *parse_expr(rparser_t *p) {
    return parse_additive(p);
}

/* ── API pública ──────────────────────────────────────────── */

/**
 * Inicia a análise sintática de uma string de texto.
 * Retorna a raiz da árvore AST gerada.
 */
ast_node_t *rparser_run(rparser_t *p, const char *src) {
    p->ok = 1;
    p->lx->init_src(p->lx, src); // Inicializa o Lexer com a string
    
    ast_node_t *root = parse_expr(p); // Começa a descida recursiva
    
    // Se após terminar a expressão ainda houver tokens, é um erro de sintaxe
    if (!check(p, TOK_EOF)) {
        fprintf(stderr, "[PARSER] Erro: token inesperado apos expressao\n");
        p->ok = 0;
    }
    return root;
}

/**
 * Construtor do Parser: vincula o Lexer necessário para ler os tokens.
 */
void rparser_create(rparser_t *p, lexer_t *lx) {
    p->lx         = lx;
    p->ok         = 1;
    p->parse_expr = parse_expr;
}