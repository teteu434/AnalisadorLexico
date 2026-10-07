/* =========================================================================
 * lexer.h
 * -------------------------------------------------------------------------
 * Analisador Lexico orientado a estados (Automato Finito Deterministico).
 *
 * O fonte inteiro e carregado em memoria, o que torna o lookahead e o
 * retrocesso (retract) triviais: basta avancar/recuar o indice 'pos'.
 * A funcao get_next_token() executa um unico laco de transicao de estados
 * ate atingir um estado final, momento em que devolve o Token montado.
 * ========================================================================= */
#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdio.h>
#include "token.h"
#include "symbol_table.h"

/* -------------------------------------------------------------------------
 * Lexer - estado completo da varredura.
 * ------------------------------------------------------------------------- */
typedef struct {
    char        *source;      /* buffer com o fonte inteiro (termina em \0) */
    size_t       length;      /* tamanho do buffer em bytes                 */
    size_t       pos;         /* posicao corrente de leitura                */
    int          line;        /* linha corrente (1-based)                   */
    int          column;      /* coluna corrente (1-based)                  */
    int          error_count; /* total de erros lexicos encontrados         */
    int          token_count; /* total de tokens validos reconhecidos       */
    const char  *filename;    /* nome do arquivo, para mensagens            */
    SymbolTable *symbols;     /* tabela de simbolos associada               */
    int          report_errors; /* 1 = imprime erros em stderr na hora      */
    FILE        *error_stream;  /* destino dos erros (default: stderr)      */
} Lexer;

/* -------------------------------------------------------------------------
 * Ciclo de vida
 * ------------------------------------------------------------------------- */

/* Carrega o arquivo inteiro em memoria e prepara o lexer.
 * Retorna 1 em sucesso; 0 se o arquivo nao abriu ou faltou memoria. */
int lexer_init_file(Lexer *lx, const char *filename, SymbolTable *st);

/* Variante que analisa uma string ja residente em memoria (util em testes).
 * A string e copiada; o chamador mantem a posse da original. */
int lexer_init_string(Lexer *lx, const char *source, SymbolTable *st);

/* Libera o buffer do fonte e zera a estrutura. */
void lexer_free(Lexer *lx);

/* -------------------------------------------------------------------------
 * Varredura
 * ------------------------------------------------------------------------- */

/* Reconhece e devolve o proximo token do fonte.
 *
 * Ao final do arquivo devolve, indefinidamente, um token TOK_EOF.
 * Em caso de erro lexico devolve um token TOK_ERROR cujo campo .message
 * descreve o problema e cujo .line indica onde ele ocorreu; o lexer entao
 * se recupera (descartando o trecho ofensivo) e segue, de modo que uma
 * unica execucao liste TODOS os erros do programa fonte - exigencia do
 * relatorio. Para abortar no primeiro erro, basta o chamador parar o laco
 * ao receber TOK_ERROR. */
Token get_next_token(Lexer *lx);

/* -------------------------------------------------------------------------
 * Consultas
 * ------------------------------------------------------------------------- */

int lexer_error_count(const Lexer *lx);
int lexer_token_count(const Lexer *lx);

#endif /* LEXER_H */
