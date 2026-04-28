# 🚀 Pipeline NDR → Neander

Este projeto implementa um pipeline completo que traduz expressões matemáticas de uma linguagem de alto nível para execução na máquina Neander.

---

## 🚀 Fluxo de Operação

O sistema opera seguindo as seguintes fases:

* **Frontend**
  Extrai a expressão e variáveis do arquivo fonte.

* **Parser**
  Constrói uma Árvore Sintática Abstrata (AST), respeitando a precedência matemática.

* **Codegen**
  Traduz a AST para instruções em Assembly da máquina Neander.

* **Assembler**
  Converte o código Assembly em uma imagem de memória binária de 256 bytes.

* **Executor**
  Simula o ciclo **Fetch → Decode → Execute** da CPU Neander para obter o resultado final.

---

## 🛠️ Como Compilar e Executar

### 1. Preparação e Compilação

```bash
cd ndrv2
make
```

O comando `make` compila todos os módulos:

* frontend
* lexer
* parser
* codegen
* assembler
* executor

---

### 2. Execução de Testes

Execute o compilador passando um arquivo `.nasm`:

```bash
# Cálculo básico: (10 + 6) / 4 = 4
./nasm2 tests/calculo_basico.nasm

# Equação complexa: 2 * (3 + 4) - 5 = 9
./nasm2 tests/equacao.nasm
```

---

### 3. Modo Detalhado (Verbose)

Para visualizar detalhes internos do pipeline:

```bash
./nasm2 tests/equacao.nasm --verbose
```

Este modo exibe:

* Árvore Sintática (AST)
* Código Assembly gerado
* Dump da memória (hexdump)

---

### 4. Testes Automatizados

Para executar todos os testes:

```bash
make test
```

---

## 📊 Arquivos Gerados

Após a execução, o pipeline gera automaticamente:

* `<nome>.asm`
  Código Assembly gerado pelo Codegen

* `<nome>.mem`
  Imagem binária de 256 bytes pronta para execução na CPU Neander

---


