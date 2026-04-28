#include "ndr.h"

/* ── Tabela de instruções ─────────────────────────────────── */
// Define se a instrução é implícita (sem operando) ou direta (com endereço)
typedef enum { MODE_IMP, MODE_DIR } addr_mode_t;
// Estrutura que define uma instrução: mnemônico, opcode e modo de endereçamento
typedef struct { const char *mn; uint8_t op; addr_mode_t mode; } idef_t;

// Tabela contendo o conjunto de instruções suportado (ex: NOP, LDA, ADD...)
static const idef_t ITAB[] = {
    {"NOP", OP_NOP, MODE_IMP}, {"STA", OP_STA, MODE_DIR},
    {"LDA", OP_LDA, MODE_DIR}, {"ADD", OP_ADD, MODE_DIR},
    {"OR",  OP_OR,  MODE_DIR}, {"AND", OP_AND, MODE_DIR},
    {"NOT", OP_NOT, MODE_IMP}, {"JMP", OP_JMP, MODE_DIR},
    {"JN",  OP_JN,  MODE_DIR}, {"JZ",  OP_JZ,  MODE_DIR},
    {"HLT", OP_HLT, MODE_IMP}, {NULL, 0, MODE_IMP} // Terminador da tabela
};

// Busca uma instrução na ITAB pelo seu nome (mnemônico)
static const idef_t *find_instr(const char *mn) {
    for (int i = 0; ITAB[i].mn; i++)
        if (strcmp(ITAB[i].mn, mn) == 0) return &ITAB[i];
    return NULL;
}

/* ── Tabela de Símbolos ───────────────────────────────────── */
// Insere um novo rótulo (label) e seu endereço na tabela de símbolos
static int st_put(symtab_t *st, const char *name, uint8_t addr) {
    uint8_t dummy;
    // Verifica se o rótulo já existe para evitar duplicatas
    if (st->get(st, name, &dummy)) {
        fprintf(stderr, "[ASM] Rotulo duplicado: '%s'\n", name); return 0;
    }
    // Verifica se a tabela atingiu o limite máximo
    if (st->count >= MAX_SYMS) {
        fprintf(stderr, "[ASM] Tabela de simbolos cheia\n"); return 0;
    }
    // Armazena o nome e o endereço correspondente
    strncpy(st->entries[st->count].name, name, MAX_SYMLEN - 1);
    st->entries[st->count].addr = addr;
    st->count++;
    return 1;
}

// Recupera o endereço de um rótulo a partir do nome
static int st_get(symtab_t *st, const char *name, uint8_t *out) {
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->entries[i].name, name) == 0) {
            *out = st->entries[i].addr; return 1;
        }
    return 0;
}

// Exibe o conteúdo da tabela de símbolos no console (para debug)
static void st_dump(symtab_t *st) {
    printf("\n--- Tabela de Simbolos ---\n");
    printf("%-18s  Hex   Dec\n", "Rotulo");
    printf("%-18s  ---   ---\n", "------");
    for (int i = 0; i < st->count; i++)
        printf("%-18s  0x%02X  %3d\n",
               st->entries[i].name,
               st->entries[i].addr,
               st->entries[i].addr);
    printf("\n");
}

// Inicializa a estrutura da tabela de símbolos e atribui os ponteiros de função
void symtab_create(symtab_t *st) {
    st->count = 0;
    st->put   = st_put;
    st->get   = st_get;
    st->dump  = st_dump;
}

/* ── Helpers de parsing de linha asm ──────────────────────── */
// Remove tudo após o caractere ';' (comentários)
static void strip_asm_comment(char *s) {
    char *p = strchr(s, ';'); if (p) *p = '\0';
}

// Decompõe uma linha de texto em: rótulo, mnemônico e operando
static int parse_asm_line(const char *raw,
                          char *label, char *mn, char *operand) {
    char buf[512];
    strncpy(buf, raw, 511); buf[511] = '\0';
    strip_asm_comment(buf);
    str_trim(buf); // Remove espaços em branco nas extremidades
    label[0] = mn[0] = operand[0] = '\0';
    if (str_blank(buf)) return 0;

    char *p = buf, tok[64]; int i = 0;
    // Extrai o primeiro token
    while (*p && !isspace((unsigned char)*p) && *p != ':') tok[i++] = *p++;
    tok[i] = '\0';

    char up[64]; str_to_upper(up, tok);

    // Se terminar com ':', é um rótulo explícito
    if (*p == ':') {
        strcpy(label, up); p++;
        str_trim(p);
        i = 0; char mn2[64] = {0};
        while (*p && !isspace((unsigned char)*p)) mn2[i++] = *p++;
        mn2[i] = '\0';
        str_to_upper(mn, mn2);
    } else {
        // Verifica se o token é uma instrução ou diretiva; se não for, é um rótulo
        if (find_instr(up) || !strcmp(up,"DATA") || !strcmp(up,"ORG") || !strcmp(up,"SPACE"))
            strcpy(mn, up);
        else {
            strcpy(label, up);
            str_trim(p);
            i = 0; char mn2[64] = {0};
            while (*p && !isspace((unsigned char)*p)) mn2[i++] = *p++;
            mn2[i] = '\0';
            str_to_upper(mn, mn2);
        }
    }
    str_trim(p); str_to_upper(operand, p);
    return 1;
}

// Converte strings numéricas (decimal ou hexadecimal 0x) para inteiro
static int parse_num(const char *s, int *out) {
    char *end;
    long v = (s[0]=='0' && (s[1]=='x'||s[1]=='X'))
             ? strtol(s+2, &end, 16) : strtol(s, &end, 10);
    if (*end && !isspace((unsigned char)*end)) return 0;
    *out = (int)v; return 1;
}

/* ── Passagem 1 ───────────────────────────────────────────── */
// Primeira leitura: Calcula os endereços de todos os rótulos
static int pass1(assembler_t *as, const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) return 0;
    char raw[512]; int lc=0, ln=0, ok=1; // lc = Location Counter
    while (fgets(raw, sizeof(raw), fp)) {
        ln++;
        char lbl[64], mn[64], op[64];
        if (!parse_asm_line(raw, lbl, mn, op)) continue;
        
        // ORG define o endereço inicial de carregamento
        if (!strcmp(mn,"ORG")) { int a; if (parse_num(op,&a)) lc=a; continue; }
        
        // Se houver rótulo, guarda o valor atual do Location Counter na tabela
        if (lbl[0] && !as->tab.put(&as->tab, lbl, (uint8_t)lc)) ok=0;
        if (!mn[0]) continue;

        // Incrementa o LC baseado no tamanho da instrução ou diretiva
        const idef_t *id = find_instr(mn);
        if (id)             lc += (id->mode==MODE_DIR) ? 2 : 1;
        else if (!strcmp(mn,"DATA"))  lc += 1;
        else if (!strcmp(mn,"SPACE")) { int n; if(parse_num(op,&n)&&n>0) lc+=n; }
        else { fprintf(stderr,"[ASM] Linha %d: '%s' desconhecido\n",ln,mn); ok=0; }

        if (lc > MEM_SIZE) { fprintf(stderr,"[ASM] Linha %d: overflow\n",ln); ok=0; break; }
    }
    fclose(fp); return ok;
}

/* ── Passagem 2 ───────────────────────────────────────────── */
// Segunda leitura: Gera o código binário real (opcodes e operandos)
static int pass2(assembler_t *as, const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) return 0;
    char raw[512]; int lc=0, ln=0, ok=1;
    while (fgets(raw, sizeof(raw), fp)) {
        ln++;
        char lbl[64], mn[64], op[64];
        if (!parse_asm_line(raw, lbl, mn, op)) continue;
        if (!strcmp(mn,"ORG")) { int a; if(parse_num(op,&a)) lc=a; continue; }
        if (!mn[0]) continue;

        const idef_t *id = find_instr(mn);
        if (id) {
            as->mem[lc++] = id->op; // Escreve o Opcode na memória
            if (id->mode == MODE_DIR) { // Se tiver operando de endereço
                uint8_t ab=0; int num;
                if (parse_num(op,&num)) ab=(uint8_t)num; // Valor numérico direto
                else if (op[0]) {
                    // Resolve o nome do rótulo para o endereço real salvo na Passagem 1
                    if (!as->tab.get(&as->tab,op,&ab)) {
                        fprintf(stderr,"[ASM] Linha %d: simbolo indefinido '%s'\n",ln,op); ok=0;
                    }
                } else { fprintf(stderr,"[ASM] Linha %d: operando faltando\n",ln); ok=0; }
                as->mem[lc++] = ab; // Escreve o endereço/dado na memória
            }
        } else if (!strcmp(mn,"DATA")) { // Diretiva para inserir dado bruto
            int v=0;
            if (op[0]) {
                if (!parse_num(op,&v)) {
                    uint8_t sa; if(as->tab.get(&as->tab,op,&sa)) v=sa;
                    else { fprintf(stderr,"[ASM] Linha %d: operando invalido\n",ln); ok=0; }
                }
            }
            as->mem[lc++]=(uint8_t)v;
        } else if (!strcmp(mn,"SPACE")) { // Reserva N bytes vazios
            int n; if(parse_num(op,&n)) lc+=n;
        }
    }
    fclose(fp); return ok;
}

/* ── Métodos públicos ─────────────────────────────────────── */
// Função principal de montagem
static int as_assemble(assembler_t *as, const char *path) {
    memset(as->mem, 0, MEM_SIZE); as->err = 0;
    printf("\n=== Assembler (2 passagens) ===\n");
    printf("[Passagem 1] Mapeando rotulos...\n");
    if (!pass1(as, path)) { as->err++; return 0; }
    
    as->tab.dump(&as->tab); // Mostra o mapa de endereços criado
    
    printf("[Passagem 2] Gerando opcodes...\n");
    if (!pass2(as, path)) { as->err++; return 0; }
    
    printf("Montagem concluida sem erros.\n");
    return 1;
}

// Exibe a memória resultante em formato hexadecimal (tabela 16x16)
static void as_dump_mem(assembler_t *as) {
    printf("\n--- Hexdump de Memoria ---\n    ");
    for(int c=0;c<16;c++) printf(" %02X",c);
    printf("\n    "); for(int c=0;c<16;c++) printf(" --"); printf("\n");
    for(int r=0;r<16;r++){
        printf("%02X |",r*16);
        for(int c=0;c<16;c++) printf(" %02X",as->mem[r*16+c]);
        printf("\n");
    }
}

// Salva o binário gerado em um arquivo físico
static void as_save(assembler_t *as, const char *path) {
    FILE *fp = fopen(path,"wb");
    if (!fp) { fprintf(stderr,"[ASM] Erro ao salvar '%s'\n",path); return; }
    fwrite(as->mem,1,MEM_SIZE,fp); fclose(fp);
    printf("[ASM] Imagem binaria salva em '%s'\n", path);
}

// Construtor do objeto assembler
void assembler_create(assembler_t *as) {
    memset(as->mem,0,MEM_SIZE); as->err=0;
    symtab_create(&as->tab);
    as->assemble = as_assemble;
    as->dump_mem = as_dump_mem;
    as->save     = as_save;
}