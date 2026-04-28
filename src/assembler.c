#include "ndr.h"

/* ── Tabela de instruções ─────────────────────────────────── */
typedef enum { MODE_IMP, MODE_DIR } addr_mode_t;
typedef struct { const char *mn; uint8_t op; addr_mode_t mode; } idef_t;

static const idef_t ITAB[] = {
    {"NOP", OP_NOP, MODE_IMP}, {"STA", OP_STA, MODE_DIR},
    {"LDA", OP_LDA, MODE_DIR}, {"ADD", OP_ADD, MODE_DIR},
    {"OR",  OP_OR,  MODE_DIR}, {"AND", OP_AND, MODE_DIR},
    {"NOT", OP_NOT, MODE_IMP}, {"JMP", OP_JMP, MODE_DIR},
    {"JN",  OP_JN,  MODE_DIR}, {"JZ",  OP_JZ,  MODE_DIR},
    {"HLT", OP_HLT, MODE_IMP}, {NULL, 0, MODE_IMP}
};

static const idef_t *find_instr(const char *mn) {
    for (int i = 0; ITAB[i].mn; i++)
        if (strcmp(ITAB[i].mn, mn) == 0) return &ITAB[i];
    return NULL;
}

/* ── Tabela de Símbolos ───────────────────────────────────── */
static int st_put(symtab_t *st, const char *name, uint8_t addr) {
    uint8_t dummy;
    if (st->get(st, name, &dummy)) {
        fprintf(stderr, "[ASM] Rotulo duplicado: '%s'\n", name); return 0;
    }
    if (st->count >= MAX_SYMS) {
        fprintf(stderr, "[ASM] Tabela de simbolos cheia\n"); return 0;
    }
    strncpy(st->entries[st->count].name, name, MAX_SYMLEN - 1);
    st->entries[st->count].addr = addr;
    st->count++;
    return 1;
}

static int st_get(symtab_t *st, const char *name, uint8_t *out) {
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->entries[i].name, name) == 0) {
            *out = st->entries[i].addr; return 1;
        }
    return 0;
}

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

void symtab_create(symtab_t *st) {
    st->count = 0;
    st->put   = st_put;
    st->get   = st_get;
    st->dump  = st_dump;
}

/* ── Helpers de parsing de linha asm ──────────────────────── */
static void strip_asm_comment(char *s) {
    char *p = strchr(s, ';'); if (p) *p = '\0';
}

static int parse_asm_line(const char *raw,
                          char *label, char *mn, char *operand) {
    char buf[512];
    strncpy(buf, raw, 511); buf[511] = '\0';
    strip_asm_comment(buf);
    str_trim(buf);
    label[0] = mn[0] = operand[0] = '\0';
    if (str_blank(buf)) return 0;

    char *p = buf, tok[64]; int i = 0;
    while (*p && !isspace((unsigned char)*p) && *p != ':') tok[i++] = *p++;
    tok[i] = '\0';

    char up[64]; str_to_upper(up, tok);

    if (*p == ':') {
        strcpy(label, up); p++;
        str_trim(p);
        i = 0; char mn2[64] = {0};
        while (*p && !isspace((unsigned char)*p)) mn2[i++] = *p++;
        mn2[i] = '\0';
        str_to_upper(mn, mn2);
    } else {
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

static int parse_num(const char *s, int *out) {
    char *end;
    long v = (s[0]=='0' && (s[1]=='x'||s[1]=='X'))
             ? strtol(s+2, &end, 16) : strtol(s, &end, 10);
    if (*end && !isspace((unsigned char)*end)) return 0;
    *out = (int)v; return 1;
}

/* ── Passagem 1 ───────────────────────────────────────────── */
static int pass1(assembler_t *as, const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) return 0;
    char raw[512]; int lc=0, ln=0, ok=1;
    while (fgets(raw, sizeof(raw), fp)) {
        ln++;
        char lbl[64], mn[64], op[64];
        if (!parse_asm_line(raw, lbl, mn, op)) continue;
        if (!strcmp(mn,"ORG")) { int a; if (parse_num(op,&a)) lc=a; continue; }
        if (lbl[0] && !as->tab.put(&as->tab, lbl, (uint8_t)lc)) ok=0;
        if (!mn[0]) continue;
        const idef_t *id = find_instr(mn);
        if (id)              lc += (id->mode==MODE_DIR) ? 2 : 1;
        else if (!strcmp(mn,"DATA"))  lc += 1;
        else if (!strcmp(mn,"SPACE")) { int n; if(parse_num(op,&n)&&n>0) lc+=n; }
        else { fprintf(stderr,"[ASM] Linha %d: '%s' desconhecido\n",ln,mn); ok=0; }
        if (lc > MEM_SIZE) { fprintf(stderr,"[ASM] Linha %d: overflow\n",ln); ok=0; break; }
    }
    fclose(fp); return ok;
}

/* ── Passagem 2 ───────────────────────────────────────────── */
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
            as->mem[lc++] = id->op;
            if (id->mode == MODE_DIR) {
                uint8_t ab=0; int num;
                if (parse_num(op,&num)) ab=(uint8_t)num;
                else if (op[0]) {
                    if (!as->tab.get(&as->tab,op,&ab)) {
                        fprintf(stderr,"[ASM] Linha %d: simbolo indefinido '%s'\n",ln,op); ok=0;
                    }
                } else { fprintf(stderr,"[ASM] Linha %d: operando faltando\n",ln); ok=0; }
                as->mem[lc++] = ab;
            }
        } else if (!strcmp(mn,"DATA")) {
            int v=0;
            if (op[0]) {
                if (!parse_num(op,&v)) {
                    uint8_t sa; if(as->tab.get(&as->tab,op,&sa)) v=sa;
                    else { fprintf(stderr,"[ASM] Linha %d: operando invalido\n",ln); ok=0; }
                }
            }
            as->mem[lc++]=(uint8_t)v;
        } else if (!strcmp(mn,"SPACE")) {
            int n; if(parse_num(op,&n)) lc+=n;
        }
    }
    fclose(fp); return ok;
}

/* ── Métodos públicos ─────────────────────────────────────── */
static int as_assemble(assembler_t *as, const char *path) {
    memset(as->mem, 0, MEM_SIZE); as->err = 0;
    printf("\n=== Assembler (2 passagens) ===\n");
    printf("[Passagem 1] Mapeando rotulos...\n");
    if (!pass1(as, path)) { as->err++; return 0; }
    as->tab.dump(&as->tab);
    printf("[Passagem 2] Gerando opcodes...\n");
    if (!pass2(as, path)) { as->err++; return 0; }
    printf("Montagem concluida sem erros.\n");
    return 1;
}

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

static void as_save(assembler_t *as, const char *path) {
    FILE *fp = fopen(path,"wb");
    if (!fp) { fprintf(stderr,"[ASM] Erro ao salvar '%s'\n",path); return; }
    fwrite(as->mem,1,MEM_SIZE,fp); fclose(fp);
    printf("[ASM] Imagem binaria salva em '%s'\n", path);
}

void assembler_create(assembler_t *as) {
    memset(as->mem,0,MEM_SIZE); as->err=0;
    symtab_create(&as->tab);
    as->assemble = as_assemble;
    as->dump_mem = as_dump_mem;
    as->save     = as_save;
}
