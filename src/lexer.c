#include "ndr.h"

/**
 * Inicializa o Lexer com uma string de origem (o código/expressão).
 * Define a posição inicial como zero e já carrega o primeiro token.
 */
static void lx_init_src(lexer_t *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->next(lx); /* Faz o "bootstrap": carrega o primeiro token em lx->current */
}

/**
 * A função principal do Lexer: lê o próximo Token da string.
 * Ela identifica números, operadores e ignora espaços em branco.
 */
static tok_t lx_next(lexer_t *lx) {
    /* Pula espaços em branco, tabs e quebras de linha */
    while (lx->src[lx->pos] && isspace((unsigned char)lx->src[lx->pos]))
        lx->pos++;

    char c = lx->src[lx->pos];

    // Inicializa um token padrão como Fim de Arquivo (EOF)
    tok_t t = { TOK_EOF, 0 };

    /* Se chegamos ao fim da string, retorna o token de EOF */
    if (c == '\0') { lx->current = t; return t; }

    /* Identificação de Números: se o caractere for um dígito (0-9) */
    if (isdigit((unsigned char)c)) {
        long v = 0;
        // Percorre a string enquanto houver dígitos para formar o número completo
        while (isdigit((unsigned char)lx->src[lx->pos]))
            v = v * 10 + (lx->src[lx->pos++] - '0');
        
        t.kind  = TOK_NUM;
        t.value = v;
        lx->current = t; // Atualiza o token atual do lexer
        return t;
    }

    /* Identificação de Símbolos e Operadores */
    lx->pos++; // Avança a posição para o próximo caractere
    switch (c) {
        case '+': t.kind = TOK_PLUS;    break;
        case '-': t.kind = TOK_MINUS;   break;
        case '*': t.kind = TOK_STAR;    break;
        case '/': t.kind = TOK_SLASH;   break;
        case '%': t.kind = TOK_PERCENT; break;
        case '(': t.kind = TOK_LPAREN;  break;
        case ')': t.kind = TOK_RPAREN;  break;
        default:  t.kind = TOK_ERROR;   break; // Caractere desconhecido
    }
    
    t.value = (long)c; // Guarda o caractere original como valor para depuração
    lx->current = t;   // Atualiza o token atual do lexer
    return t;
}

/**
 * Apenas "espia" o token que já foi processado e está em 'current',
 * sem avançar a leitura na string original.
 */
static tok_t lx_peek(lexer_t *lx) {
    return lx->current;
}

/**
 * Construtor do Lexer: inicializa a estrutura e mapeia os ponteiros
 * de função para os métodos estáticos acima.
 */
void lexer_create(lexer_t *lx) {
    lx->src      = NULL;
    lx->pos      = 0;
    lx->init_src = lx_init_src;
    lx->next     = lx_next;
    lx->peek     = lx_peek;
}