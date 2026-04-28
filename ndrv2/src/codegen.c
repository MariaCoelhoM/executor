#include "ndr.h"

/* ── Emissores internos ───────────────────────────────────── */
// Adiciona uma linha de instrução ao buffer de código final
static void emit_code(codegen_t *cg, const char *fmt, ...) {
    if (cg->code_cnt >= MAX_ASM_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(cg->code[cg->code_cnt++], MAX_ASM_LINE, fmt, ap);
    va_end(ap);
}

// Adiciona uma definição de variável à seção de dados (DATA)
static void emit_data(codegen_t *cg, const char *fmt, ...) {
    if (cg->data_cnt >= MAX_DATA_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(cg->data[cg->data_cnt++], MAX_ASM_LINE, fmt, ap);
    va_end(ap);
}

// Gera um nome único para variáveis temporárias (ex: _t0, _t1...)
static void new_tmp(codegen_t *cg, char *out) {
    snprintf(out, 16, "_t%d", cg->tmp_cnt++);
}

// Gera nomes únicos para rótulos de desvio/loop (ex: _La0, _Lb0...)
static void new_labels(codegen_t *cg, char *lbl_a, char *lbl_b) {
    int id = cg->lbl_cnt++;
    snprintf(lbl_a, 16, "_La%d", id);
    snprintf(lbl_b, 16, "_Lb%d", id);
}

/*
 * Geração recursiva pós-ordem.
 * Varre a árvore e gera o código Neander para resolver a expressão.
 */
static void gen(codegen_t *cg, ast_node_t *node, char *result_var) {
    if (!node) { result_var[0] = '\0'; return; }

    /* ── Número folha ────────────────────────────────────── */
    // Se o nó for um número, cria uma variável DATA para ele
    if (node->kind == AST_NUM) {
        new_tmp(cg, result_var);
        emit_data(cg, "%-14s DATA %d", result_var, (int)(node->value & 0xFF));
        return;
    }

    /* ── Unário ──────────────────────────────────────────── */
    // Trata sinais de negativo (ex: -5)
    if (node->kind == AST_UNARY) {
        char op_var[16];
        gen(cg, node->left, op_var);
        if ((char)node->value == '-') {
            char one[16], res[16];
            new_tmp(cg, one);
            new_tmp(cg, res);
            emit_data(cg, "%-14s DATA 1",  one); // Neander precisa de um '1' na memória para somar
            emit_data(cg, "%-14s DATA 0",  res);
            emit_code(cg, "        ; neg(%s)", op_var);
            emit_code(cg, "        LDA  %s", op_var);
            emit_code(cg, "        NOT");           // Inverte os bits (Complemento de 1)
            emit_code(cg, "        ADD  %s", one);    // Soma 1 (Complemento de 2)
            emit_code(cg, "        STA  %s", res);
            strcpy(result_var, res);
        } else {
            strcpy(result_var, op_var); /* unário + : não faz nada */
        }
        return;
    }

    /* ── Binário ─────────────────────────────────────────── */
    // Processa os dois lados da operação antes de aplicar o operador
    char lv[16], rv[16];
    gen(cg, node->left,  lv);
    gen(cg, node->right, rv);

    char res[16];
    new_tmp(cg, res);

    char op = (char)node->value;
    emit_code(cg, "        ; %s %c %s", lv, op, rv);

    switch (op) {

        case '+':
            emit_data(cg, "%-14s DATA 0", res);
            emit_code(cg, "        LDA  %s", lv);
            emit_code(cg, "        ADD  %s", rv);
            emit_code(cg, "        STA  %s", res);
            break;

        case '-': {
            /* Subtração via complemento de 2: A - B = A + (~B + 1) */
            char one[16], nb[16];
            new_tmp(cg, one); new_tmp(cg, nb);
            emit_data(cg, "%-14s DATA 1", one);
            emit_data(cg, "%-14s DATA 0", nb);
            emit_data(cg, "%-14s DATA 0", res);
            emit_code(cg, "        LDA  %s", rv);
            emit_code(cg, "        NOT");
            emit_code(cg, "        ADD  %s", one);
            emit_code(cg, "        STA  %s", nb);
            emit_code(cg, "        LDA  %s", lv);
            emit_code(cg, "        ADD  %s", nb);
            emit_code(cg, "        STA  %s", res);
            break;
        }

        case '*': {
            /* Multiplicação por soma repetida (Neander não tem MUL) */
            char acc[16], cnt[16], neg1[16];
            char lbl_loop[16], lbl_fim[16];
            new_tmp(cg, acc); new_tmp(cg, cnt); new_tmp(cg, neg1);
            new_labels(cg, lbl_loop, lbl_fim);

            emit_data(cg, "%-14s DATA 0",   acc);
            emit_data(cg, "%-14s DATA 0",   cnt);
            emit_data(cg, "%-14s DATA 255", neg1); // 255 em 8 bits é o mesmo que -1
            emit_data(cg, "%-14s DATA 0",   res);

            emit_code(cg, "        LDA  %s",    rv);
            emit_code(cg, "        STA  %s",    cnt);
            emit_code(cg, "%s:", lbl_loop);
            emit_code(cg, "        LDA  %s",    cnt);
            emit_code(cg, "        JZ   %s",    lbl_fim); // Se contador for 0, para
            emit_code(cg, "        LDA  %s",    acc);
            emit_code(cg, "        ADD  %s",    lv);
            emit_code(cg, "        STA  %s",    acc);
            emit_code(cg, "        LDA  %s",    cnt);
            emit_code(cg, "        ADD  %s",    neg1);    // Decrementa contador
            emit_code(cg, "        STA  %s",    cnt);
            emit_code(cg, "        JMP  %s",    lbl_loop);
            emit_code(cg, "%s:", lbl_fim);
            emit_code(cg, "        LDA  %s",    acc);
            emit_code(cg, "        STA  %s",    res);
            break;
        }

        case '/':
        case '%': {
            /* Divisão por subtração repetida */
            char quot[16], divid[16], um[16], nb[16];
            char lbl_loop[16], lbl_fim[16];
            new_tmp(cg, quot); new_tmp(cg, divid);
            new_tmp(cg, um);   new_tmp(cg, nb);
            new_labels(cg, lbl_loop, lbl_fim);

            emit_data(cg, "%-14s DATA 0", quot);
            emit_data(cg, "%-14s DATA 0", divid);
            emit_data(cg, "%-14s DATA 1", um);
            emit_data(cg, "%-14s DATA 0", nb);
            emit_data(cg, "%-14s DATA 0", res);

            // Transforma B em -B para subtrair usando ADD
            emit_code(cg, "        LDA  %s", rv);
            emit_code(cg, "        NOT");
            emit_code(cg, "        ADD  %s", um);
            emit_code(cg, "        STA  %s", nb);
            
            emit_code(cg, "        LDA  %s", lv);
            emit_code(cg, "        STA  %s", divid);

            emit_code(cg, "%s:", lbl_loop);
            emit_code(cg, "        LDA  %s", divid);
            emit_code(cg, "        ADD  %s", nb);     // Tenta subtrair
            emit_code(cg, "        JN   %s", lbl_fim); // Se ficar negativo, terminou
            emit_code(cg, "        STA  %s", divid);   // Atualiza o resto
            emit_code(cg, "        LDA  %s", quot);
            emit_code(cg, "        ADD  %s", um);      // Incrementa quociente
            emit_code(cg, "        STA  %s", quot);
            emit_code(cg, "        JMP  %s", lbl_loop);
            emit_code(cg, "%s:", lbl_fim);

            // Se for '/', pega o quociente. Se for '%', pega o que sobrou (divid)
            if (op == '/') {
                emit_code(cg, "        LDA  %s", quot);
            } else {
                emit_code(cg, "        LDA  %s", divid);
            }
            emit_code(cg, "        STA  %s", res);
            break;
        }

        default:
            fprintf(stderr, "[CODEGEN] Operador desconhecido: '%c'\n", op);
            cg->ok = 0;
    }

    strcpy(result_var, res);
}

/* ── API pública ─────────────────────────────────────────── */
// Função principal que inicia a geração do código
static int cg_gen(codegen_t *cg, ast_node_t *root, const char *var) {
    cg->code_cnt = cg->data_cnt = cg->tmp_cnt = cg->lbl_cnt = 0;
    cg->ok = 1;

    emit_code(cg, "        ORG  0"); // Início do programa no endereço 0
    emit_code(cg, "");

    char result[16];
    gen(cg, root, result); // Começa a recursão pela raiz da AST
    if (!cg->ok) return 0;

    emit_code(cg, "        ; guardar resultado em %s", var);
    emit_code(cg, "        LDA  %s", result);
    emit_code(cg, "        STA  %s", var);
    emit_code(cg, "        HLT"); // Instrução de parada
    emit_code(cg, "");
    emit_code(cg, "; ── secao de dados ──────────────────────────");

    // Despeja todas as variáveis DATA criadas durante a geração
    for (int i = 0; i < cg->data_cnt; i++)
        emit_code(cg, "%s", cg->data[i]);

    char var_upper[MAX_VARNAME];
    str_to_upper(var_upper, var);
    
    // Reserva espaço para a variável de saída final
    emit_code(cg, "%-14s DATA 0", var);
    return 1;
}

// Grava o código assembly gerado em um arquivo .asm
static void cg_write(codegen_t *cg, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) { fprintf(stderr, "[CODEGEN] Erro ao criar '%s'\n", path); return; }
    fprintf(fp, "; Gerado automaticamente — compilador NASM v2\n");
    fprintf(fp, "; Pipeline: .nasm -> lexer -> parser -> AST -> codegen -> .asm\n\n");
    for (int i = 0; i < cg->code_cnt; i++)
        fprintf(fp, "%s\n", cg->code[i]);
    fclose(fp);
    printf("[CODEGEN] '%s' escrito (%d linhas)\n", path, cg->code_cnt);
}

// Mostra o assembly na tela para depuração
static void cg_show(codegen_t *cg) {
    printf("\n=== Assembly Gerado ===\n");
    for (int i = 0; i < cg->code_cnt; i++)
        printf("%s\n", cg->code[i]);
}

// Inicializa a estrutura do gerador de código
void codegen_create(codegen_t *cg) {
    cg->code_cnt = cg->data_cnt = cg->tmp_cnt = cg->lbl_cnt = 0;
    cg->ok    = 1;
    cg->gen   = cg_gen;
    cg->write = cg_write;
    cg->show  = cg_show;
}