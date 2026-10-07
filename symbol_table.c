/* =========================================================================
 * symbol_table.c - tabela hash com encadeamento separado.
 * ========================================================================= */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "symbol_table.h"

/* -------------------------------------------------------------------------
 * Palavras reservadas da linguagem (secao 4 da especificacao: "As
 * palavras-chave sao reservadas"). Sao instaladas na tabela durante
 * symtab_init(), antes de qualquer leitura do fonte.
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *lexeme;
    TokenType   type;
} ReservedWord;

static const ReservedWord RESERVED_WORDS[] = {
    { "program", TOK_PROGRAM },
    { "begin",   TOK_BEGIN   },
    { "end",     TOK_END     },
    { "int",     TOK_INT     },
    { "float",   TOK_FLOAT   },
    { "char",    TOK_CHAR    },
    { "if",      TOK_IF      },
    { "then",    TOK_THEN    },
    { "else",    TOK_ELSE    },
    { "repeat",  TOK_REPEAT  },
    { "until",   TOK_UNTIL   },
    { "while",   TOK_WHILE   },
    { "do",      TOK_DO      },
    { "read",    TOK_READ    },
    { "write",   TOK_WRITE   }
};

static const int RESERVED_COUNT =
    (int)(sizeof RESERVED_WORDS / sizeof RESERVED_WORDS[0]);

/* -------------------------------------------------------------------------
 * symtab_hash - djb2 (Daniel J. Bernstein).
 *
 * Nao normaliza caixa: a linguagem e case-sensitive, portanto "Write" e
 * "write" produzem entradas distintas - exatamente o comportamento
 * desejado (em "Write(...)" o Write e um identificador, nao a palavra
 * reservada write).
 * ------------------------------------------------------------------------- */
unsigned long symtab_hash(const char *lexeme)
{
    unsigned long hash = 5381UL;
    const unsigned char *p = (const unsigned char *)lexeme;

    while (*p != 0) {
        hash = ((hash << 5) + hash) + (unsigned long)(*p); /* hash*33 + c */
        p++;
    }
    return hash % (unsigned long)SYMTAB_BUCKETS;
}

/* -------------------------------------------------------------------------
 * new_symbol - aloca e preenche uma entrada (ainda sem encadea-la).
 * ------------------------------------------------------------------------- */
static Symbol *new_symbol(const char *lexeme, TokenType type,
                          int index, int line)
{
    Symbol *sym;
    size_t  len;

    sym = (Symbol *)malloc(sizeof *sym);
    if (sym == NULL) return NULL;

    len = strlen(lexeme);
    sym->lexeme = (char *)malloc(len + 1);
    if (sym->lexeme == NULL) {
        free(sym);
        return NULL;
    }
    memcpy(sym->lexeme, lexeme, len + 1);

    sym->type        = type;
    sym->index       = index;
    sym->first_line  = line;
    sym->occurrences = 0;
    sym->next        = NULL;
    sym->order_next  = NULL;
    return sym;
}

/* -------------------------------------------------------------------------
 * insert_symbol - insere uma entrada nova (assume lexema ainda inexistente).
 * Atualiza buckets, lista de ordem de insercao e estatisticas.
 * ------------------------------------------------------------------------- */
static Symbol *insert_symbol(SymbolTable *st, const char *lexeme,
                             TokenType type, int line)
{
    unsigned long  h;
    Symbol        *sym;

    sym = new_symbol(lexeme, type, st->count, line);
    if (sym == NULL) return NULL;

    h = symtab_hash(lexeme);
    if (st->buckets[h] != NULL) st->collisions++;
    sym->next      = st->buckets[h];
    st->buckets[h] = sym;

    if (st->order_head == NULL) st->order_head = sym;
    else                        st->order_tail->order_next = sym;
    st->order_tail = sym;

    st->count++;
    return sym;
}

int symtab_init(SymbolTable *st)
{
    int i;

    if (st == NULL) return 0;

    for (i = 0; i < SYMTAB_BUCKETS; i++) st->buckets[i] = NULL;
    st->order_head = NULL;
    st->order_tail = NULL;
    st->count      = 0;
    st->collisions = 0;

    /* Pre-carga das palavras reservadas. */
    for (i = 0; i < RESERVED_COUNT; i++) {
        if (insert_symbol(st, RESERVED_WORDS[i].lexeme,
                          RESERVED_WORDS[i].type, 0) == NULL) {
            symtab_free(st);
            return 0;
        }
    }
    return 1;
}

void symtab_free(SymbolTable *st)
{
    Symbol *cur, *next;
    int     i;

    if (st == NULL) return;

    /* Percorre a lista de ordem: cada entrada aparece nela exatamente 1 vez. */
    cur = st->order_head;
    while (cur != NULL) {
        next = cur->order_next;
        free(cur->lexeme);
        free(cur);
        cur = next;
    }

    for (i = 0; i < SYMTAB_BUCKETS; i++) st->buckets[i] = NULL;
    st->order_head = NULL;
    st->order_tail = NULL;
    st->count      = 0;
    st->collisions = 0;
}

Symbol *symtab_lookup(const SymbolTable *st, const char *lexeme)
{
    unsigned long  h;
    Symbol        *cur;

    if (st == NULL || lexeme == NULL) return NULL;

    h   = symtab_hash(lexeme);
    cur = st->buckets[h];
    while (cur != NULL) {
        if (strcmp(cur->lexeme, lexeme) == 0) return cur;  /* case-sensitive */
        cur = cur->next;
    }
    return NULL;
}

Symbol *symtab_install(SymbolTable *st, const char *lexeme, int line)
{
    Symbol *sym;

    if (st == NULL || lexeme == NULL) return NULL;

    sym = symtab_lookup(st, lexeme);
    if (sym == NULL) {
        /* Lexema inedito: so pode ser identificador, pois todas as palavras
         * reservadas foram pre-carregadas em symtab_init(). */
        sym = insert_symbol(st, lexeme, TOK_ID, line);
        if (sym == NULL) return NULL;
    } else if (sym->first_line == 0) {
        /* Primeira ocorrencia real de uma palavra reservada no fonte. */
        sym->first_line = line;
    }

    sym->occurrences++;
    return sym;
}

/* -------------------------------------------------------------------------
 * Impressao
 * ------------------------------------------------------------------------- */
static void print_rows(const SymbolTable *st, FILE *stream, int only_ids)
{
    const Symbol *cur;

    fprintf(stream, "%-6s  %-24s  %-14s  %-14s  %-6s  %s\n",
            "INDICE", "LEXEMA", "TOKEN", "CLASSE", "LINHA", "USOS");
    fprintf(stream,
            "------  ------------------------  --------------  "
            "--------------  ------  ----\n");

    for (cur = st->order_head; cur != NULL; cur = cur->order_next) {
        const char *classe;

        if (only_ids && cur->type != TOK_ID) continue;

        classe = token_is_reserved(cur->type) ? "PALAVRA-RESV" : "IDENTIFICADOR";

        if (cur->first_line > 0) {
            fprintf(stream, "%-6d  %-24s  %-14s  %-14s  %-6d  %d\n",
                    cur->index, cur->lexeme, token_type_name(cur->type),
                    classe, cur->first_line, cur->occurrences);
        } else {
            /* Palavra reservada pre-carregada que nao ocorreu no fonte. */
            fprintf(stream, "%-6d  %-24s  %-14s  %-14s  %-6s  %d\n",
                    cur->index, cur->lexeme, token_type_name(cur->type),
                    classe, "-", cur->occurrences);
        }
    }
}

void symtab_print(const SymbolTable *st, FILE *stream)
{
    if (st == NULL) return;
    if (stream == NULL) stream = stdout;

    print_rows(st, stream, 0);
    fprintf(stream, "\nTotal de simbolos: %d  (colisoes de hash: %d)\n",
            st->count, st->collisions);
}

void symtab_print_identifiers(const SymbolTable *st, FILE *stream)
{
    const Symbol *cur;
    int n = 0;

    if (st == NULL) return;
    if (stream == NULL) stream = stdout;

    print_rows(st, stream, 1);
    for (cur = st->order_head; cur != NULL; cur = cur->order_next)
        if (cur->type == TOK_ID) n++;

    fprintf(stream, "\nTotal de identificadores: %d\n", n);
}
