/* =========================================================================
 * symbol_table.h
 * -------------------------------------------------------------------------
 * Tabela de Simbolos implementada como tabela hash com encadeamento
 * separado (separate chaining) e alocacao dinamica.
 *
 * Caracteristicas:
 *   - Pre-carregada com as 15 palavras reservadas da linguagem.
 *   - Identificadores sao instalados sob demanda pelo analisador lexico.
 *   - Mantem, alem dos buckets, uma lista de ordem de insercao, de modo
 *     que a impressao da tabela seja deterministica (util no relatorio).
 *   - Busca case-sensitive, conforme exigido pela especificacao.
 * ========================================================================= */
#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdio.h>
#include "token.h"

/* Numero de buckets. Primo, para melhor dispersao do hash. */
#define SYMTAB_BUCKETS 211

/* -------------------------------------------------------------------------
 * Symbol - entrada da Tabela de Simbolos.
 *
 * 'next'       encadeia colisoes dentro de um mesmo bucket.
 * 'order_next' encadeia TODAS as entradas na ordem em que foram instaladas.
 * ------------------------------------------------------------------------- */
typedef struct Symbol {
    char           *lexeme;      /* copia propria do lexema (malloc)        */
    TokenType       type;        /* TOK_ID ou a palavra reservada especifica */
    int             index;       /* indice de instalacao (0, 1, 2, ...)      */
    int             first_line;  /* linha da primeira ocorrencia             */
    int             occurrences; /* quantas vezes foi reconhecido            */
    struct Symbol  *next;        /* proximo no mesmo bucket                  */
    struct Symbol  *order_next;  /* proximo na ordem de insercao             */
} Symbol;

/* -------------------------------------------------------------------------
 * SymbolTable - a tabela propriamente dita.
 * ------------------------------------------------------------------------- */
typedef struct {
    Symbol *buckets[SYMTAB_BUCKETS];
    Symbol *order_head;   /* primeira entrada instalada */
    Symbol *order_tail;   /* ultima entrada instalada   */
    int     count;        /* total de entradas          */
    int     collisions;   /* colisoes de hash (estatistica) */
} SymbolTable;

/* -------------------------------------------------------------------------
 * Ciclo de vida
 * ------------------------------------------------------------------------- */

/* Inicializa a tabela e instala as palavras reservadas.
 * Retorna 1 em caso de sucesso, 0 se faltar memoria. */
int  symtab_init(SymbolTable *st);

/* Libera toda a memoria das entradas e reinicializa os campos. */
void symtab_free(SymbolTable *st);

/* -------------------------------------------------------------------------
 * Consulta e insercao
 * ------------------------------------------------------------------------- */

/* Procura 'lexeme' (case-sensitive). Retorna a entrada ou NULL. */
Symbol *symtab_lookup(const SymbolTable *st, const char *lexeme);

/* Procura 'lexeme'; se nao existir, instala como TOK_ID.
 *
 * Este e o ponto de entrada usado pelo analisador lexico: como as palavras
 * reservadas ja estao pre-carregadas, um lexema que "casa" com uma delas
 * retorna a entrada reservada e o lexer apenas copia sym->type. Assim a
 * distincao identificador x palavra reservada sai de graca da tabela.
 *
 * 'line' registra a primeira ocorrencia e incrementa o contador de usos.
 * Retorna NULL apenas em caso de falha de alocacao. */
Symbol *symtab_install(SymbolTable *st, const char *lexeme, int line);

/* -------------------------------------------------------------------------
 * Relatorio
 * ------------------------------------------------------------------------- */

/* Imprime a tabela completa (palavras reservadas + identificadores)
 * na ordem de instalacao. */
void symtab_print(const SymbolTable *st, FILE *stream);

/* Imprime apenas os identificadores instalados pelo programa fonte. */
void symtab_print_identifiers(const SymbolTable *st, FILE *stream);

/* Funcao de hash (djb2 restrito ao numero de buckets). Exposta para testes. */
unsigned long symtab_hash(const char *lexeme);

#endif /* SYMBOL_TABLE_H */
