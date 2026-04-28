#include "ndr.h"

/**
 * Atualiza as flags de condição (N e Z).
 * Flag N (Negative): Ligada se o bit mais significativo (bit 7) for 1.
 * Flag Z (Zero): Ligada se o Acumulador (AC) for exatamente 0.
 */
static void upd_flags(cpu_t *c) {
    c->flagN = (c->AC & 0x80) ? 1 : 0;
    c->flagZ = (c->AC == 0)   ? 1 : 0;
}

// Copia o programa binário para a memória principal do simulador
static void ex_load_mem(executor_t *ex, uint8_t *src) {
    memcpy(ex->mem, src, MEM_SIZE);
}

// Reseta os registradores e flags da CPU para o estado inicial
static void ex_reset(executor_t *ex) {
    ex->cpu.AC = ex->cpu.PC = ex->cpu.IR = 0;
    ex->cpu.MAR = ex->cpu.MDR = 0;
    ex->cpu.flagN = 0; ex->cpu.flagZ = 1; // Z começa em 1 pois AC inicia em 0
    ex->cpu.cycles = 0; ex->cpu.halted = 0;
}

/**
 * Executa um único passo (instrução) da CPU.
 * Implementa o ciclo de Fetch (Busca) e Execute (Execução).
 */
static int ex_step(executor_t *ex) {
    cpu_t *c = &ex->cpu;
    if (c->halted) return 0;

    /* --- FASE DE BUSCA (FETCH) --- */
    c->MAR = c->PC;               // Endereço do PC vai para o MAR
    c->MDR = ex->mem[c->MAR];     // Busca a instrução na memória
    c->IR  = c->MDR;              // Guarda no Registrador de Instrução
    c->PC++;                      // Incrementa o contador de programa
    c->cycles++;

    /* --- FASE DE DECODIFICAÇÃO / EXECUÇÃO --- */
    switch (c->IR) {
        case OP_NOP: break; // Nenhuma operação

        case OP_LDA: // Load AC: Carrega valor da memória no Acumulador
            c->MAR = ex->mem[c->PC++]; // Busca o endereço do operando
            c->MDR = ex->mem[c->MAR];   // Busca o valor no endereço
            c->AC  = c->MDR;            // Coloca no Acumulador
            upd_flags(c);
            break;

        case OP_STA: // Store AC: Salva o valor do Acumulador na memória
            c->MAR = ex->mem[c->PC++];
            ex->mem[c->MAR] = c->AC;
            break;

        case OP_ADD: // Add: Soma valor da memória com o Acumulador
            c->MAR = ex->mem[c->PC++];
            c->MDR = ex->mem[c->MAR];
            c->AC  = (uint8_t)(c->AC + c->MDR);
            upd_flags(c);
            break;

        case OP_OR: // Logical OR: AC = AC | Memoria
            c->MAR = ex->mem[c->PC++];
            c->MDR = ex->mem[c->MAR];
            c->AC  = c->AC | c->MDR;
            upd_flags(c);
            break;

        case OP_AND: // Logical AND: AC = AC & Memoria
            c->MAR = ex->mem[c->PC++];
            c->MDR = ex->mem[c->MAR];
            c->AC  = c->AC & c->MDR;
            upd_flags(c);
            break;

        case OP_NOT: // Inverte todos os bits do Acumulador
            c->AC = (uint8_t)(~c->AC);
            upd_flags(c);
            break;

        case OP_JMP: // Salto incondicional para endereço
            c->MAR = ex->mem[c->PC];
            c->PC  = c->MAR;
            break;

        case OP_JN: // Salto se a flag N (negativo) estiver ligada
            c->MAR = ex->mem[c->PC++];
            if (c->flagN) c->PC = c->MAR;
            break;

        case OP_JZ: // Salto se a flag Z (zero) estiver ligada
            c->MAR = ex->mem[c->PC++];
            if (c->flagZ) c->PC = c->MAR;
            break;

        case OP_HLT: // Para a CPU
            c->halted = 1;
            printf("[CPU] HLT — encerrado em %llu ciclos.\n",
                   (unsigned long long)c->cycles);
            return 0;

        default: // Caso encontre um código que não existe
            fprintf(stderr, "[CPU] Opcode desconhecido: 0x%02X (PC=0x%02X)\n",
                    c->IR, (uint8_t)(c->PC - 1));
            c->halted = 1;
            return 0;
    }
    return 1;
}

// Executa o programa continuamente até encontrar um HLT ou estourar o limite
static void ex_run(executor_t *ex) {
    uint64_t lim = (uint64_t)MEM_SIZE * MEM_SIZE * 32;
    while (!ex->cpu.halted && ex->cpu.cycles < lim)
        ex_step(ex);
    if (!ex->cpu.halted)
        printf("[CPU] Limite de ciclos atingido sem HLT.\n");
}

// Exibe visualmente o estado final de todos os registradores
static void ex_show(executor_t *ex) {
    cpu_t *c = &ex->cpu;
    printf("\n+------------------------------------------+\n");
    printf("|        Estado Final — CPU Neander        |\n");
    printf("+--------------------+---------------------+\n");
    printf("| AC  = 0x%02X  (%4d) | bin: %c%c%c%c%c%c%c%c         |\n",
           c->AC, (int8_t)c->AC,
           (c->AC>>7)&1?'1':'0',(c->AC>>6)&1?'1':'0',
           (c->AC>>5)&1?'1':'0',(c->AC>>4)&1?'1':'0',
           (c->AC>>3)&1?'1':'0',(c->AC>>2)&1?'1':'0',
           (c->AC>>1)&1?'1':'0',(c->AC>>0)&1?'1':'0');
    printf("| PC  = 0x%02X          | IR  = 0x%02X           |\n", c->PC, c->IR);
    printf("| MAR = 0x%02X          | MDR = 0x%02X           |\n", c->MAR, c->MDR);
    printf("| N=%d  Z=%d           | ciclos: %-11llu|\n",
           c->flagN, c->flagZ, (unsigned long long)c->cycles);
    printf("+--------------------+---------------------+\n");
}

// Exibe o conteúdo de toda a memória (256 bytes) em formato hexadecimal
static void ex_hexdump(executor_t *ex) {
    printf("\n--- Hexdump Pos-Execucao ---\n    ");
    for(int c=0;c<16;c++) printf(" %02X",c);
    printf("\n    "); for(int c=0;c<16;c++) printf(" --"); printf("\n");
    for(int r=0;r<16;r++){
        printf("%02X |",r*16);
        for(int c=0;c<16;c++) printf(" %02X",ex->mem[r*16+c]);
        printf("\n");
    }
}

// Inicializa a estrutura do executor e mapeia as funções
void executor_create(executor_t *ex) {
    memset(ex->mem, 0, MEM_SIZE);
    ex_reset(ex);
    ex->load_mem = ex_load_mem;
    ex->reset    = ex_reset;
    ex->step     = ex_step;
    ex->run      = ex_run;
    ex->show     = ex_show;
    ex->hexdump  = ex_hexdump;
}