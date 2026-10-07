# =========================================================================
# Makefile - Compilador Etapa 1 (Analisador Lexico e Tabela de Simbolos)
#
#   make            -> gera o executavel
#   make clean      -> remove objetos e executavel
#   make run FILE=x -> compila e analisa o arquivo x
# =========================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
TARGET  = compilador
OBJS    = main.o lexer.o symbol_table.o token.o

ifeq ($(OS),Windows_NT)
    EXE = $(TARGET).exe
    RM  = del /Q
else
    EXE = $(TARGET)
    RM  = rm -f
endif

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o:         main.c         token.h symbol_table.h lexer.h
lexer.o:        lexer.c        token.h symbol_table.h lexer.h
symbol_table.o: symbol_table.c token.h symbol_table.h
token.o:        token.c        token.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXE)
	./$(EXE) $(FILE)

clean:
	-$(RM) $(OBJS) $(EXE)

.PHONY: all run clean
