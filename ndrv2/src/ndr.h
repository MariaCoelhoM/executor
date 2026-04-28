#ifndef NDR_H
#define NDR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════
 *  Constantes da Máquina Neander
 * ═══════════════════════════════════════════════════════ */
#define MEM_SIZE   256
#define OP_NOP     0x00
#define OP_STA     0x10
#define OP_LDA     0x20
#define OP_ADD     0x30
#define OP_OR      0x50
#define OP_AND     0x60
#define OP_NOT     0x70
#define OP_JMP     0x80
#define OP_JN      0x90
#define OP_JZ      0xA0
#define OP_HLT     0xF0

/* ═══════════════════════════════════════════════════════
 *  Tipos de Token
 * ═══════════════════════════════════════════════════════ */
typedef enum {
    TOK_NUM,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
    TOK_ERROR
} tok_kind_t;

typedef struct {
    tok_kind_t kind;
    long       value;  /* usado quando kind == TOK_NUM */
} tok_t;

/* ═══════════════════════════════════════════════════════
 *  Lexer (Tokenizador)
 * ═══════════════════════════════════════════════════════ */
typedef struct lexer {
    const char *src;
    int         pos;
    tok_t       current;
    /* métodos */
    void  (*init_src)(struct lexer*, const char *src);
    tok_t (*next)    (struct lexer*);
    tok_t (*peek)    (struct lexer*);
} lexer_t;

void lexer_create(lexer_t *lx);

/* ═══════════════════════════════════════════════════════
 *  AST
 * ═══════════════════════════════════════════════════════ */
typedef enum {
    AST_NUM,
    AST_BINOP,
    AST_UNARY
} ast_kind_t;

typedef struct ast_node {
    ast_kind_t      kind;
    long            value;    /* AST_NUM: número; AST_BINOP/UNARY: operador char */
    struct ast_node *left;
    struct ast_node *right;
} ast_node_t;

ast_node_t *ast_num  (long value);
ast_node_t *ast_binop(char op, ast_node_t *l, ast_node_t *r);
ast_node_t *ast_unary(char op, ast_node_t *operand);
void        ast_free (ast_node_t *n);
void        ast_print(ast_node_t *n, int depth);
long        ast_eval (ast_node_t *n, int *ok);

/* ═══════════════════════════════════════════════════════
 *  Parser (Descendente Recursivo)
 * ═══════════════════════════════════════════════════════ */
typedef struct rparser {
    lexer_t  *lx;
    int       ok;
    /* métodos */
    ast_node_t *(*parse_expr)(struct rparser*);
} rparser_t;

void        rparser_create(rparser_t *p, lexer_t *lx);
ast_node_t *rparser_run   (rparser_t *p, const char *src);

/* ═══════════════════════════════════════════════════════
 *  Frontend — lê o arquivo .nasm
 * ═══════════════════════════════════════════════════════ */
#define MAX_PROGNAME  64
#define MAX_VARNAME   64
#define MAX_EXPR      512

typedef struct {
    char prog_name[MAX_PROGNAME];
    char var_name [MAX_VARNAME];
    char expr     [MAX_EXPR];
    int  valid;
} nasm_prog_t;

typedef struct frontend {
    nasm_prog_t prog;
    int  (*load) (struct frontend*, const char *path);
    void (*show)  (struct frontend*);
} frontend_t;

void frontend_create(frontend_t *fe);

/* ═══════════════════════════════════════════════════════
 *  Gerador de Código — AST → .asm Neander
 * ═══════════════════════════════════════════════════════ */
#define MAX_ASM_LINES  1024
#define MAX_ASM_LINE   80
#define MAX_DATA_LINES 128

typedef struct codegen {
    char code[MAX_ASM_LINES][MAX_ASM_LINE];
    int  code_cnt;
    char data[MAX_DATA_LINES][MAX_ASM_LINE];
    int  data_cnt;
    int  tmp_cnt;
    int  lbl_cnt;
    int  ok;
    /* métodos */
    int  (*gen)  (struct codegen*, ast_node_t *root, const char *var);
    void (*write) (struct codegen*, const char *path);
    void (*show)  (struct codegen*);
} codegen_t;

void codegen_create(codegen_t *cg);

/* ═══════════════════════════════════════════════════════
 *  Assembler — duas passagens
 * ═══════════════════════════════════════════════════════ */
#define MAX_SYMS   128
#define MAX_SYMLEN  64

typedef struct { char name[MAX_SYMLEN]; uint8_t addr; } sym_t;

typedef struct symtab {
    sym_t entries[MAX_SYMS];
    int   count;
    int  (*put)  (struct symtab*, const char *name, uint8_t addr);
    int  (*get)  (struct symtab*, const char *name, uint8_t *out);
    void (*dump) (struct symtab*);
} symtab_t;

void symtab_create(symtab_t *st);

typedef struct assembler {
    symtab_t tab;
    uint8_t  mem[MEM_SIZE];
    int      err;
    /* métodos */
    int  (*assemble)(struct assembler*, const char *path);
    void (*dump_mem)(struct assembler*);
    void (*save)    (struct assembler*, const char *path);
} assembler_t;

void assembler_create(assembler_t *as);

/* ═══════════════════════════════════════════════════════
 *  Executor — simulador CPU Neander
 * ═══════════════════════════════════════════════════════ */
typedef struct {
    uint8_t  AC, PC, IR, MAR, MDR;
    uint8_t  flagN, flagZ;
    uint64_t cycles;
    int      halted;
} cpu_t;

typedef struct executor {
    cpu_t   cpu;
    uint8_t mem[MEM_SIZE];
    /* métodos */
    void (*load_mem)(struct executor*, uint8_t *src);
    void (*reset)   (struct executor*);
    int  (*step)    (struct executor*);
    void (*run)     (struct executor*);
    void (*show)    (struct executor*);
    void (*hexdump) (struct executor*);
} executor_t;

void executor_create(executor_t *ex);

/* ═══════════════════════════════════════════════════════
 *  Utilitários
 * ═══════════════════════════════════════════════════════ */
void   str_to_upper(char *dst, const char *src);
void   str_trim    (char *s);
int    str_blank   (const char *s);

#endif /* NDR_H */
