# Relatório Técnico — Pipeline 
## Assembler e Executor para a Máquina Neander
---

## 1. Introdução

Este relatório documenta o desenvolvimento do compilador NASM v2, um pipeline completo que traduz expressões matemáticas escritas em uma linguagem de alto nível criada especificamente para este projeto até a execução simulada na Máquina Neander.

O pipeline é composto por cinco etapas encadeadas:

| Etapa | Descrição |
|---|---|
| **1 – Frontend** | Lê o arquivo `.nasm`, extrai nome do programa, variável de saída e a expressão matemática. Valida a sintaxe da linguagem NASM. |
| **2 – Lexer + Parser** | O Lexer converte a expressão em tokens. O Parser Descendente Recursivo constrói a AST respeitando precedência e parênteses. |
| **3 – Codegen** | Percorre a AST em pós-ordem e emite código assembly Neander com variáveis temporárias (`_t0`, `_t1`...). Suporta `+`, `-`, `*`, `/`, `%` e negação unária. |
| **4 – Assembler** | Duas passagens: a 1ª mapeia rótulos na tabela de símbolos; a 2ª traduz mnemônicos e resolve endereços, gerando a imagem binária `.mem`. |
| **5 – Executor** | Simula a CPU Neander completa (Fetch→Decode→Execute), atualiza AC, PC, IR, MAR, MDR e flags N/Z, e exibe o estado final. |

Exemplo de programa NASM:

```
# comentário
programa calculo_basico
  resultado quociente = (10 + 6) / 4
fim
```

---

## 2. Estrutura do Assembler

O assembler traduz o arquivo `.asm` (gerado pelo Codegen) para a imagem binária de memória `.mem`. Opera em duas passagens distintas sobre o mesmo arquivo fonte.

### 2.1 Conjunto de Instruções Suportado

| Mnemônico | Opcode | Modo | Descrição |
|---|---|---|---|
| `NOP` | `0x00` | Implícito | Nenhuma operação |
| `LDA end` | `0x20` | Direto | `AC ← mem[end]` |
| `STA end` | `0x10` | Direto | `mem[end] ← AC` |
| `ADD end` | `0x30` | Direto | `AC ← AC + mem[end]` |
| `OR end` | `0x50` | Direto | `AC ← AC OR mem[end]` |
| `AND end` | `0x60` | Direto | `AC ← AC AND mem[end]` |
| `NOT` | `0x70` | Implícito | `AC ← ~AC` |
| `JMP end` | `0x80` | Direto | `PC ← end` (incondicional) |
| `JN end` | `0x90` | Direto | Se `N=1`: `PC ← end` |
| `JZ end` | `0xA0` | Direto | Se `Z=1`: `PC ← end` |
| `HLT` | `0xF0` | Implícito | Encerra a execução |

### 2.2 Primeira Passagem — Construção da Tabela de Símbolos

A primeira passagem varre o arquivo `.asm` linha a linha **sem gerar código**. Seu objetivo é calcular os endereços de todos os rótulos e armazená-los na tabela de símbolos.

O algoritmo mantém um **Location Counter (LC)** inicializado em zero. Para cada linha:

- **`ORG <addr>`:** redefine o LC para o endereço especificado.
- **Instrução modo direto** (2 bytes): `LC += 2`.
- **Instrução modo implícito** (1 byte): `LC += 1`.
- **`DATA`:** `LC += 1`.
- **`SPACE <n>`:** `LC += n` (bytes inicializados com zero).
- **Rótulo detectado:** associa o nome ao LC atual e insere na tabela.

### 2.3 Tabela de Símbolos

A tabela de símbolos é implementada como um array estático de até 128 entradas com busca linear. Cada entrada armazena o nome do rótulo (até 63 caracteres) e seu endereço de 8 bits.

A estrutura `symtab_t` expõe três métodos via ponteiros de função:

- **`put(name, addr)`:** insere novo símbolo, detectando duplicatas.
- **`get(name, *out)`:** busca pelo nome e retorna o endereço.
- **`dump()`:** imprime a tabela completa para depuração.

Qualquer tentativa de redefinir um rótulo existente gera erro e aborta a montagem.

### 2.4 Segunda Passagem — Geração de Código de Máquina

A segunda passagem usa a tabela de símbolos para resolver todos os endereços simbólicos e gerar os bytes finais:

- **Mnemônico de instrução:** escreve o opcode em `mem[LC++]`.
- **Instrução modo direto:** resolve o operando (número literal ou rótulo via `get()`) e escreve em `mem[LC++]`.
- **`DATA <val>`:** escreve o valor em `mem[LC++]`.
- **`SPACE <n>`:** avança o LC em n posições.

Ao final, a imagem de 256 bytes é salva no formato binário plano `.mem`.

### 2.5 Tratamento de Erros

O assembler detecta e reporta:

- Rótulo duplicado na primeira passagem.
- Símbolo indefinido na segunda passagem.
- Mnemônico desconhecido.
- Operando ausente em instrução que requer endereço.
- Overflow de memória (programa excede 256 bytes).

---

## 3. O Executor (Simulador da CPU Neander)

O Executor simula completamente a CPU Neander, incluindo registradores, acesso à memória e flags de condição. Recebe a imagem `.mem` e executa instrução a instrução até `HLT` ou o limite de ciclos de segurança.

### 3.1 Modelo dos Registradores

| Registrador | Tamanho | Função |
|---|---|---|
| **AC** (Acumulador) | 8 bits | Registrador principal — operandos e resultados aritméticos/lógicos |
| **PC** (Program Counter) | 8 bits | Endereço da próxima instrução a buscar na memória |
| **IR** (Instruction Register) | 8 bits | Armazena o opcode da instrução em execução |
| **MAR** | 8 bits | Endereço da posição de memória acessada |
| **MDR** | 8 bits | Dado lido ou a ser escrito na memória |
| **Flag N** | 1 bit | Negativo: ativado quando bit 7 do AC = 1 |
| **Flag Z** | 1 bit | Zero: ativado quando `AC = 0x00` |

### 3.2 Ciclo de Máquina — Fetch → Decode → Execute

Cada chamada a `step()` executa um ciclo completo:

#### Fase FETCH (Busca)

```
MAR    ← PC
MDR    ← mem[MAR]
IR     ← MDR
PC     ← PC + 1
ciclos ← ciclos + 1
```

#### Fase DECODE / EXECUTE

A decodificação é realizada em um `switch` sobre o opcode no IR:

- **`LDA end`** — `MAR ← mem[PC++]`; `AC ← mem[MAR]`. Atualiza N e Z.
- **`STA end`** — `MAR ← mem[PC++]`; `mem[MAR] ← AC`. Não altera flags.
- **`ADD end`** — `AC ← AC + mem[end]` com aritmética módulo 256. Atualiza N e Z.
- **`NOT`** — `AC ← ~AC`. Atualiza N e Z.
- **`OR end` / `AND end`** — operação bit a bit com `mem[end]`. Atualiza N e Z.
- **`JMP end`** — `PC ← mem[PC]`. Desvio incondicional.
- **`JN end`** — salta se `N = 1` (AC negativo).
- **`JZ end`** — salta se `Z = 1` (AC = 0).
- **`HLT`** — `halted = 1`, encerra o laço de execução.

### 3.3 Manipulação das Flags N e Z

Após qualquer instrução que modifica o AC (`LDA`, `ADD`, `OR`, `AND`, `NOT`), a função `upd_flags()` é chamada:

```c
flagN = (AC & 0x80) ? 1 : 0;   // bit 7 = 1 → sinal negativo (complemento de 2)
flagZ = (AC == 0x00) ? 1 : 0;  // AC zerado → flag zero
```

Valores de `0x80` (128) a `0xFF` (255) são interpretados como negativos. As instruções `STA`, `JMP`, `JN` e `JZ` **não** alteram as flags.

### 3.4 Modo de Execução Contínua

O método `run()` chama `step()` em loop até `halted = 1` ou o limite de `256 × 256 × 32 = 2.097.152` ciclos, evitando loops infinitos em programas sem `HLT`.

---

## 4. O Gerador de Código (Codegen)

O Codegen percorre a AST em pós-ordem e emite instruções Neander para cada nó. Como a Neander não possui registradores intermediários, toda operação binária usa o padrão:

```
LDA  operando_esquerdo
<operação>
STA  _tN               ; temporário de resultado
```

### 4.1 Estratégias por Operador

| Operador | Estratégia |
|---|---|
| `+` | `LDA esq → ADD dir → STA _tN` |
| `-` | Calcula `-B` via `NOT B + ADD 1` (complemento de dois), depois `LDA A → ADD (-B) → STA _tN` |
| `*` | Loop de soma repetida com `JZ` para encerrar quando contador = 0 |
| `/` | Loop de subtração repetida com `JN` para encerrar quando dividendo < divisor |
| `%` | Igual à divisão, mas retorna o dividendo restante ao fim do loop |
| `-x` (unário) | `NOT x + ADD 1` — complemento de dois de 8 bits |

---

## 5. Resultados dos Testes

| Arquivo | Expressão | Esperado | Obtido | Ciclos |
|---|---|:---:|:---:|:---:|
| `calculo_basico.nasm` | `(10 + 6) / 4` | 4 | **4** ✅ | 49 |
| `equacao.nasm` | `2 * (3 + 4) - 5` | 9 | **9** ✅ | 82 |

Em ambos os casos o Executor produziu o resultado correto, confirmando o funcionamento integrado de todas as etapas do pipeline.

---

## 6. Conclusão

O pipeline NASM v2 demonstra na prática o funcionamento de um compilador simplificado, desde a leitura de uma linguagem de alto nível com sintaxe própria até a simulação do hardware. A implementação em C com ponteiros de função em structs mantém modularidade e clara separação de responsabilidades.

O assembler de duas passagens resolve corretamente *forward references* e detecta erros de símbolo indefinido e rótulo duplicado. O executor reproduz fielmente o ciclo Fetch-Decode-Execute da Máquina Neander, com atualização correta das flags N e Z e suporte completo ao conjunto de instruções definido.
