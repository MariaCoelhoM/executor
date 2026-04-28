CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g
TARGET = nasm2
SRCDIR = src

SRCS = $(SRCDIR)/main.c     \
       $(SRCDIR)/utils.c    \
       $(SRCDIR)/lexer.c    \
       $(SRCDIR)/ast.c      \
       $(SRCDIR)/parser.c   \
       $(SRCDIR)/frontend.c \
       $(SRCDIR)/codegen.c  \
       $(SRCDIR)/assembler.c\
       $(SRCDIR)/executor.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean test test_verbose

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/ndr.h
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(TARGET) tests/*.asm tests/*.mem

test: all
	@echo ""
	@echo "[TESTE 1] calculo_basico.nasm"
	@echo "-----------------------------"
	./$(TARGET) tests/calculo_basico.nasm
	@echo ""
	@echo "[TESTE 2] equacao.nasm"
	@echo "----------------------"
	./$(TARGET) tests/equacao.nasm

test_verbose: all
	@echo ""
	@echo "[TESTE 1 - verbose]"
	./$(TARGET) tests/calculo_basico.nasm --verbose
	@echo ""
	@echo "[TESTE 2 - verbose]"
	./$(TARGET) tests/equacao.nasm --verbose
