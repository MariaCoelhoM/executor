#include "ndr.h"

/* ── Emissores internos ───────────────────────────────────── */
static void emit_code(codegen_t *cg, const char *fmt, ...) {
    if (cg->code_cnt >= MAX_ASM_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(cg->code[cg->code_cnt++], MAX_ASM_LINE, fmt, ap);
    va_end(ap);
}

static void emit_data(codegen_t *cg, const char *fmt, ...) {
    if (cg->data_cnt >= MAX_DATA_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(cg->data[cg->data_cnt++], MAX_ASM_LINE, fmt, ap);
    va_end(ap);
}

static void new_tmp(codegen_t *cg, char *out) {
    snprintf(out, 16, "_t%d", cg->tmp_cnt++);
}

static void new_labels(codegen_t *cg, char *lbl_a, char *lbl_b) {
    int id = cg->lbl_cnt++;
    snprintf(lbl_a, 16, "_La%d", id);
    snprintf(lbl_b, 16, "_Lb%d", id);
}

/*
 * Geração recursiva pós-ordem.
 * result_var ← nome da variável temporária com o resultado deste nó.
 */
static void gen(codegen_t *cg, ast_node_t *node, char *result_var) {
    if (!node) { result_var[0] = '\0'; return; }

    /* ── Número folha ────────────────────────────────────── */
    if (node->kind == AST_NUM) {
        new_tmp(cg, result_var);
        emit_data(cg, "%-14s DATA %d", result_var, (int)(node->value & 0xFF));
        return;
    }

    /* ── Unário ──────────────────────────────────────────── */
    if (node->kind == AST_UNARY) {
        char op_var[16];
        gen(cg, node->left, op_var);
        if ((char)node->value == '-') {
            char one[16], res[16];
            new_tmp(cg, one);
            new_tmp(cg, res);
            emit_data(cg, "%-14s DATA 1",  one);
            emit_data(cg, "%-14s DATA 0",  res);
            emit_code(cg, "        ; neg(%s)", op_var);
            emit_code(cg, "        LDA  %s", op_var);
            emit_code(cg, "        NOT");
            emit_code(cg, "        ADD  %s", one);
            emit_code(cg, "        STA  %s", res);
            strcpy(result_var, res);
        } else {
            strcpy(result_var, op_var); /* unário + : identidade */
        }
        return;
    }

    /* ── Binário ─────────────────────────────────────────── */
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
            /* A - B = A + (~B + 1) */
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
            /* soma repetida: res=0; cnt=B; loop: if cnt==0 fim; res+=A; cnt--; */
            char acc[16], cnt[16], neg1[16];
            char lbl_loop[16], lbl_fim[16];
            new_tmp(cg, acc); new_tmp(cg, cnt); new_tmp(cg, neg1);
            new_labels(cg, lbl_loop, lbl_fim);

            emit_data(cg, "%-14s DATA 0",   acc);
            emit_data(cg, "%-14s DATA 0",   cnt);
            emit_data(cg, "%-14s DATA 255", neg1);
            emit_data(cg, "%-14s DATA 0",   res);

            emit_code(cg, "        LDA  %s",    rv);
            emit_code(cg, "        STA  %s",    cnt);
            emit_code(cg, "%s:", lbl_loop);
            emit_code(cg, "        LDA  %s",    cnt);
            emit_code(cg, "        JZ   %s",    lbl_fim);
            emit_code(cg, "        LDA  %s",    acc);
            emit_code(cg, "        ADD  %s",    lv);
            emit_code(cg, "        STA  %s",    acc);
            emit_code(cg, "        LDA  %s",    cnt);
            emit_code(cg, "        ADD  %s",    neg1);
            emit_code(cg, "        STA  %s",    cnt);
            emit_code(cg, "        JMP  %s",    lbl_loop);
            emit_code(cg, "%s:", lbl_fim);
            emit_code(cg, "        LDA  %s",    acc);
            emit_code(cg, "        STA  %s",    res);
            break;
        }

        case '/':
        case '%': {
            /* divisão por subtração repetida
               quociente = 0; dividendo = A
               loop: if (dividendo - B) < 0 goto fim
                     dividendo -= B; quociente++
               fim:  resultado = quociente (/) ou dividendo (%) */
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

            /* nb = ~B + 1 = -B */
            emit_code(cg, "        LDA  %s", rv);
            emit_code(cg, "        NOT");
            emit_code(cg, "        ADD  %s", um);
            emit_code(cg, "        STA  %s", nb);
            /* divid = A */
            emit_code(cg, "        LDA  %s", lv);
            emit_code(cg, "        STA  %s", divid);

            emit_code(cg, "%s:", lbl_loop);
            emit_code(cg, "        LDA  %s", divid);
            emit_code(cg, "        ADD  %s", nb);
            emit_code(cg, "        JN   %s", lbl_fim);
            emit_code(cg, "        STA  %s", divid);
            emit_code(cg, "        LDA  %s", quot);
            emit_code(cg, "        ADD  %s", um);
            emit_code(cg, "        STA  %s", quot);
            emit_code(cg, "        JMP  %s", lbl_loop);
            emit_code(cg, "%s:", lbl_fim);

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
static int cg_gen(codegen_t *cg, ast_node_t *root, const char *var) {
    cg->code_cnt = cg->data_cnt = cg->tmp_cnt = cg->lbl_cnt = 0;
    cg->ok = 1;

    emit_code(cg, "        ORG  0");
    emit_code(cg, "");

    char result[16];
    gen(cg, root, result);
    if (!cg->ok) return 0;

    emit_code(cg, "        ; guardar resultado em %s", var);
    emit_code(cg, "        LDA  %s", result);
    emit_code(cg, "        STA  %s", var);
    emit_code(cg, "        HLT");
    emit_code(cg, "");
    emit_code(cg, "; ── secao de dados ──────────────────────────");

    for (int i = 0; i < cg->data_cnt; i++)
        emit_code(cg, "%s", cg->data[i]);

    char var_upper[MAX_VARNAME];
    str_to_upper(var_upper, var);
    /* variável de saída usa o nome em minúsculo para compatibilidade */
    emit_code(cg, "%-14s DATA 0", var);
    return 1;
}

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

static void cg_show(codegen_t *cg) {
    printf("\n=== Assembly Gerado ===\n");
    for (int i = 0; i < cg->code_cnt; i++)
        printf("%s\n", cg->code[i]);
}

void codegen_create(codegen_t *cg) {
    cg->code_cnt = cg->data_cnt = cg->tmp_cnt = cg->lbl_cnt = 0;
    cg->ok    = 1;
    cg->gen   = cg_gen;
    cg->write = cg_write;
    cg->show  = cg_show;
}
