#include "ndr.h"

/*
 * Sintaxe do arquivo .nasm esperada por este Frontend:
 *
 * # comentário (ignorado)
 * programa <nome>
 * resultado <variavel> = <expressao>
 * fim
 */

/**
 * Carrega e processa o arquivo fonte .nasm.
 * Retorna 1 em caso de sucesso e 0 se houver erro de sintaxe ou abertura.
 */
static int fe_load(frontend_t *fe, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[FRONTEND] Nao foi possivel abrir '%s'\n", path);
        return 0;
    }

    // Inicializa os dados do programa com valores vazios/nulos
    fe->prog.valid = 0;
    fe->prog.prog_name[0] = '\0';
    fe->prog.var_name[0]  = '\0';
    fe->prog.expr[0]      = '\0';

    char line[1024];
    int  lineno      = 0;   // Contador de linhas para mensagens de erro
    int  in_prog     = 0;   // Flag para saber se já lemos a palavra 'programa'
    int  found_res   = 0;   // Flag para verificar se a linha 'resultado' existe

    // Lê o arquivo linha por linha
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        
        // Remove a quebra de linha (\n) e espaços inúteis nas extremidades
        line[strcspn(line, "\n")] = '\0';
        str_trim(line);

        /* Ignora linhas que estão em branco ou que são apenas comentários */
        if (str_blank(line) || line[0] == '#') continue;

        if (!in_prog) {
            /* ESTADO INICIAL: Espera encontrar a palavra-chave 'programa' */
            if (strncmp(line, "programa", 8) == 0) {
                char *rest = line + 8; // Pula a palavra 'programa'
                str_trim(rest);        // O que sobra deve ser o nome do programa
                
                if (str_blank(rest)) {
                    fprintf(stderr, "[FRONTEND] Linha %d: nome do programa ausente\n", lineno);
                    fclose(fp); return 0;
                }
                strncpy(fe->prog.prog_name, rest, MAX_PROGNAME - 1);
                in_prog = 1; // Agora estamos dentro do bloco do programa
            } else {
                fprintf(stderr, "[FRONTEND] Linha %d: 'programa' esperado\n", lineno);
                fclose(fp); return 0;
            }
        } else if (strcmp(line, "fim") == 0) {
            /* ESTADO FINAL: Encontrou a palavra 'fim', encerra a leitura */
            break; 
        } else {
            /* ESTADO INTERMEDIÁRIO: Espera encontrar 'resultado <var> = <expr>' */
            if (strncmp(line, "resultado", 9) != 0) {
                fprintf(stderr, "[FRONTEND] Linha %d: 'resultado' ou 'fim' esperado\n", lineno);
                fclose(fp); return 0;
            }
            
            char *rest = line + 9; // Pula 'resultado'
            str_trim(rest);

            /* Localiza o caractere '=' para separar variável da expressão */
            char *eq = strchr(rest, '=');
            if (!eq) {
                fprintf(stderr, "[FRONTEND] Linha %d: '=' nao encontrado\n", lineno);
                fclose(fp); return 0;
            }

            // Calcula o comprimento do nome da variável (antes do '=')
            int vlen = (int)(eq - rest);
            if (vlen <= 0 || vlen >= MAX_VARNAME) {
                fprintf(stderr, "[FRONTEND] Linha %d: nome de variavel invalido\n", lineno);
                fclose(fp); return 0;
            }
            
            // Copia o nome da variável para a estrutura
            strncpy(fe->prog.var_name, rest, vlen);
            fe->prog.var_name[vlen] = '\0';
            str_trim(fe->prog.var_name);

            /* O que vem depois do '=' é a expressão matemática */
            char *expr_start = eq + 1;
            str_trim(expr_start);
            if (str_blank(expr_start)) {
                fprintf(stderr, "[FRONTEND] Linha %d: expressao vazia\n", lineno);
                fclose(fp); return 0;
            }
            
            strncpy(fe->prog.expr, expr_start, MAX_EXPR - 1);
            fe->prog.expr[MAX_EXPR - 1] = '\0';
            found_res = 1; // Sucesso: encontramos a instrução principal
        }
    }

    fclose(fp);

    // Validação final: o arquivo acabou mas não achamos um 'resultado'
    if (!found_res) {
        fprintf(stderr, "[FRONTEND] Nenhuma instrucao 'resultado' encontrada\n");
        return 0;
    }

    fe->prog.valid = 1; // Tudo certo!
    return 1;
}

/**
 * Exibe no terminal as informações capturadas pelo Frontend.
 */
static void fe_show(frontend_t *fe) {
    printf("\n=== Frontend NASM ===\n");
    printf("Programa  : %s\n", fe->prog.prog_name);
    printf("Variavel  : %s\n", fe->prog.var_name);
    printf("Expressao : %s\n", fe->prog.expr);
    printf("Valido    : %s\n\n", fe->prog.valid ? "sim" : "nao");
}

/**
 * Construtor do Frontend: inicializa ponteiros de função e estado inicial.
 */
void frontend_create(frontend_t *fe) {
    fe->prog.valid = 0;
    fe->load = fe_load;
    fe->show = fe_show;
}