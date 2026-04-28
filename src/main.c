#include "ndr.h"

/*
 * Pipeline NASM v2:
 *
 *   .nasm  →  [FRONTEND]  →  expressão + variável
 *                │
 *                ▼
 *           [LEXER + PARSER]  →  AST
 *                │
 *                ▼
 *           [CODEGEN]  →  .asm
 *                │
 *                ▼
 *           [ASSEMBLER]  →  .mem
 *                │
 *                ▼
 *           [EXECUTOR]  →  resultado
 */

static void banner(void) {
    puts("+==================================================+");
    puts("|   Compilador NASM v2  —  Pipeline Neander        |");
    puts("|   .nasm -> .asm -> .mem -> execucao              |");
    puts("+==================================================+");
    puts("");
}

static void usage(const char *prog) {
    printf("Uso: %s <arquivo.nasm> [--verbose]\n", prog);
    printf("  --verbose   imprime AST, codigo assembly e hexdump\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *src_path = argv[1];
    int verbose = (argc >= 3 && strcmp(argv[2], "--verbose") == 0);

    /* Deriva caminhos intermediários */
    char base[256], asm_path[260], mem_path[260];
    strncpy(base, src_path, 255);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    snprintf(asm_path, 512, "%s.asm", base);
    snprintf(mem_path, 512, "%s.mem", base);

    banner();

    /* ── ETAPA 1: Frontend ─────────────────────────────── */
    printf("[1/5] Frontend — lendo '%s'\n", src_path);
    frontend_t fe;
    frontend_create(&fe);
    if (!fe.load(&fe, src_path)) {
        fprintf(stderr, "Abortado na etapa 1.\n"); return 1;
    }
    fe.show(&fe);

    /* ── ETAPA 2: Lexer + Parser → AST ────────────────── */
    printf("[2/5] Parser — construindo AST\n");
    lexer_t  lx;
    rparser_t rp;
    lexer_create(&lx);
    rparser_create(&rp, &lx);

    ast_node_t *root = rparser_run(&rp, fe.prog.expr);
    if (!rp.ok || !root) {
        fprintf(stderr, "Abortado na etapa 2.\n");
        ast_free(root); return 1;
    }
    printf("    Expressao: %s\n", fe.prog.expr);

    if (verbose) {
        printf("\n    --- AST ---\n");
        ast_print(root, 2);
        printf("\n");
    }

    /* Avalia para mostrar resultado esperado, depois re-parseia */
    {
        int ok = 1;
        long expected = ast_eval(root, &ok);
        if (ok) printf("    Resultado esperado: %ld\n\n", expected);
        else    printf("    Aviso: erro na avaliacao direta\n\n");
        ast_free(root);
        root = rparser_run(&rp, fe.prog.expr);
    }

    /* ── ETAPA 3: Gerador de Código ────────────────────── */
    printf("[3/5] Codegen — gerando assembly Neander\n");
    codegen_t cg;
    codegen_create(&cg);
    if (!cg.gen(&cg, root, fe.prog.var_name)) {
        fprintf(stderr, "Abortado na etapa 3.\n");
        ast_free(root); return 1;
    }
    cg.write(&cg, asm_path);
    if (verbose) cg.show(&cg);
    ast_free(root);

    /* ── ETAPA 4: Assembler ─────────────────────────────── */
    printf("\n[4/5] Assembler — montando '%s'\n", asm_path);
    assembler_t as;
    assembler_create(&as);
    if (!as.assemble(&as, asm_path)) {
        fprintf(stderr, "Abortado na etapa 4.\n"); return 1;
    }
    as.save(&as, mem_path);
    if (verbose) as.dump_mem(&as);

    /* ── ETAPA 5: Executor ──────────────────────────────── */
    printf("\n[5/5] Executor — simulando CPU Neander\n");
    executor_t ex;
    executor_create(&ex);
    ex.load_mem(&ex, as.mem);
    ex.run(&ex);
    ex.show(&ex);
    if (verbose) ex.hexdump(&ex);

    /* Lê variável de resultado da memória */
    char var_upper[MAX_VARNAME];
    str_to_upper(var_upper, fe.prog.var_name);
    uint8_t res_addr;
    if (as.tab.get(&as.tab, var_upper, &res_addr)) {
        int8_t resultado = (int8_t)ex.mem[res_addr];
        puts("\n+------------------------------------------+");
        printf("|  Programa   : %-26s|\n", fe.prog.prog_name);
        printf("|  Expressao  : %-26s|\n", fe.prog.expr);
        printf("|  Variavel   : %-26s|\n", fe.prog.var_name);
        printf("|  Resultado  : %-26d|\n", (int)resultado);
        printf("|  Endereco   : 0x%02X  (bruto: 0x%02X)         |\n",
               res_addr, ex.mem[res_addr]);
        puts("+------------------------------------------+");
    }

    printf("\nArquivos intermediarios:\n");
    printf("  %s\n", asm_path);
    printf("  %s\n", mem_path);
    puts("");

    return 0;
}
