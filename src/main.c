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

/*
 * Pipeline NASM v2:
 * Este diagrama mostra o fluxo de dados:
 * .nasm (Fonte) -> Frontend (Leitura) -> Parser (Lógica/Árvore)
 * -> Codegen (Tradução para Assembly) -> Assembler (Tradução para Binário)
 * -> Executor (Simulação da CPU) -> Resultado Final.
 */

// Imprime o cabeçalho visual do programa no console
static void banner(void) {
    puts("+==================================================+");
    puts("|   Compilador NASM v2  —  Pipeline Neander        |");
    puts("|   .nasm -> .asm -> .mem -> execucao              |");
    puts("+==================================================+");
    puts("");
}

// Explica ao usuário como usar o programa via linha de comando
static void usage(const char *prog) {
    printf("Uso: %s <arquivo.nasm> [--verbose]\n", prog);
    printf("  --verbose   imprime AST, codigo assembly e hexdump\n");
}

int main(int argc, char *argv[]) {
    // Verifica se o caminho do arquivo foi passado como argumento
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *src_path = argv[1];
    // Ativa o modo detalhado se o usuário passar a flag --verbose
    int verbose = (argc >= 3 && strcmp(argv[2], "--verbose") == 0);

    /* --- Deriva caminhos intermediários --- 
     * Pega o nome do arquivo (ex: "soma.nasm") e prepara os nomes
     * para os arquivos de saída ("soma.asm" e "soma.mem").
     */
    char base[256], asm_path[260], mem_path[260];
    strncpy(base, src_path, 255);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0'; // Remove a extensão .nasm
    
    // Formata os nomes dos arquivos de saída (Corrigido para evitar o warning de truncamento)
    snprintf(asm_path, 512, "%s.asm", base);
    snprintf(mem_path, 512, "%s.mem", base);

    banner();

    /* ── ETAPA 1: Frontend ─────────────────────────────── 
     * Lê o arquivo de entrada e extrai o nome do programa,
     * a variável de destino e a expressão matemática.
     */
    printf("[1/5] Frontend — lendo '%s'\n", src_path);
    frontend_t fe;
    frontend_create(&fe);
    if (!fe.load(&fe, src_path)) {
        fprintf(stderr, "Abortado na etapa 1.\n"); return 1;
    }
    fe.show(&fe);

    /* ── ETAPA 2: Lexer + Parser → AST ────────────────── 
     * Transforma a string da expressão (ex: "2+3") em uma Árvore
     * Sintática Abstrata (AST), que o computador entende hierarquicamente.
     */
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
        ast_print(root, 2); // Mostra a árvore de forma visual se verbose ativo
        printf("\n");
    }

    /* Avalia para mostrar resultado esperado, depois re-parseia
     * Faz um cálculo direto em C apenas para conferência antes de converter para Neander.
     */
    {
        int ok = 1;
        long expected = ast_eval(root, &ok);
        if (ok) printf("    Resultado esperado: %ld\n\n", expected);
        else    printf("    Aviso: erro na avaliacao direta\n\n");
        ast_free(root);
        // Recria a árvore para a próxima etapa, pois o eval não a destrói, 
        // mas o fluxo aqui optou por resetar.
        root = rparser_run(&rp, fe.prog.expr);
    }

    /* ── ETAPA 3: Gerador de Código ────────────────────── 
     * Traduz a árvore (AST) para a linguagem Assembly do Neander (.asm).
     * Aqui são geradas as instruções LDA, ADD, STA, etc.
     */
    printf("[3/5] Codegen — gerando assembly Neander\n");
    codegen_t cg;
    codegen_create(&cg);
    if (!cg.gen(&cg, root, fe.prog.var_name)) {
        fprintf(stderr, "Abortado na etapa 3.\n");
        ast_free(root); return 1;
    }
    cg.write(&cg, asm_path); // Salva o arquivo .asm no disco
    if (verbose) cg.show(&cg);
    ast_free(root);

    /* ── ETAPA 4: Assembler ─────────────────────────────── 
     * Pega o arquivo .asm e o transforma em código de máquina (binário .mem),
     * resolvendo endereços de memória e labels.
     */
    printf("\n[4/5] Assembler — montando '%s'\n", asm_path);
    assembler_t as;
    assembler_create(&as);
    if (!as.assemble(&as, asm_path)) {
        fprintf(stderr, "Abortado na etapa 4.\n"); return 1;
    }
    as.save(&as, mem_path); // Salva o binário final
    if (verbose) as.dump_mem(&as);

    /* ── ETAPA 5: Executor ──────────────────────────────── 
     * Carrega o binário na memória da CPU Neander simulada
     * e executa instrução por instrução.
     */
    printf("\n[5/5] Executor — simulando CPU Neander\n");
    executor_t ex;
    executor_create(&ex);
    ex.load_mem(&ex, as.mem); // Carrega a memória montada
    ex.run(&ex);              // Inicia o ciclo de busca e execução
    ex.show(&ex);             // Mostra o estado final dos registradores
    if (verbose) ex.hexdump(&ex);

    /* Lê variável de resultado da memória 
     * Busca na tabela de símbolos onde a variável de saída foi guardada
     * e lê o valor diretamente da memória final da CPU.
     */
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