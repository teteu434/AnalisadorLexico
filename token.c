/* =========================================================================
 * token.c - implementacao das rotinas de nomeacao e impressao de tokens.
 * ========================================================================= */
#include <string.h>
#include <stdio.h>
#include "token.h"

/* -------------------------------------------------------------------------
 * Tabela de nomes. A ordem DEVE acompanhar exatamente o enum TokenType.
 * ------------------------------------------------------------------------- */
static const char *const TOKEN_NAMES[TOK_TYPE_COUNT] = {
    "ID",
    /* palavras reservadas */
    "PROGRAM", "BEGIN", "END", "INT", "FLOAT", "CHAR",
    "IF", "THEN", "ELSE", "REPEAT", "UNTIL",
    "WHILE", "DO", "READ", "WRITE",
    /* constantes */
    "INT_CONST", "FLOAT_CONST", "CHAR_CONST", "LITERAL",
    /* relop */
    "EQ", "NE", "GT", "GE", "LT", "LE",
    /* addop */
    "PLUS", "MINUS", "OR",
    /* mulop */
    "MUL", "DIV", "MOD", "AND",
    /* unario / atribuicao */
    "NOT", "ASSIGN",
    /* delimitadores */
    "SEMICOLON", "COMMA", "LPAREN", "RPAREN", "DOT", "COLON",
    /* controle */
    "EOF", "ERRO_LEXICO"
};

void token_clear(Token *t)
{
    if (t == NULL) return;
    t->type         = TOK_ERROR;
    t->lexeme[0]    = '\0';
    t->line         = 0;
    t->column       = 0;
    t->symbol_index = -1;
    t->symbol       = NULL;
    t->message[0]   = '\0';
}

const char *token_type_name(TokenType type)
{
    if (type < 0 || type >= TOK_TYPE_COUNT) return "???";
    return TOKEN_NAMES[type];
}

int token_is_reserved(TokenType type)
{
    return (type >= TOK_RESERVED_FIRST && type <= TOK_RESERVED_LAST);
}

int token_is_constant(TokenType type)
{
    return (type == TOK_INT_CONST  || type == TOK_FLOAT_CONST ||
            type == TOK_CHAR_CONST || type == TOK_LITERAL);
}

/* -------------------------------------------------------------------------
 * token_format - monta <NOME_TOKEN, VALOR_ATRIBUTO>.
 *
 * Convencao adotada para o VALOR_ATRIBUTO:
 *   - ID e palavras reservadas .... indice da entrada na Tabela de Simbolos
 *                                   -> <NOME_TOKEN, indice_TS>
 *   - demais tokens ............... o proprio lexema reconhecido
 *                                   -> <NOME_TOKEN, "lexema">
 *   - EOF ......................... "-"
 *   - ERRO_LEXICO ................. a mensagem de diagnostico
 * ------------------------------------------------------------------------- */
int token_format(const Token *t, char *out, size_t out_size)
{
    int n;

    if (t == NULL || out == NULL || out_size == 0) return 0;

    if (t->type == TOK_ID || token_is_reserved(t->type)) {
        /* O indice na TS e o atributo classico de um identificador. */
        n = snprintf(out, out_size, "<%s, %d>",
                     token_type_name(t->type), t->symbol_index);
    } else if (t->type == TOK_LITERAL) {
        n = snprintf(out, out_size, "<%s, \"%s\">",
                     token_type_name(t->type), t->lexeme);
    } else if (t->type == TOK_CHAR_CONST) {
        n = snprintf(out, out_size, "<%s, '%s'>",
                     token_type_name(t->type), t->lexeme);
    } else if (t->type == TOK_EOF) {
        n = snprintf(out, out_size, "<%s, ->", token_type_name(t->type));
    } else if (t->type == TOK_ERROR) {
        n = snprintf(out, out_size, "<%s, %s>",
                     token_type_name(t->type), t->message);
    } else {
        /* constantes numericas, operadores e delimitadores: o proprio
         * lexema, entre aspas, no formato <NOME_TOKEN, "lexema"> */
        n = snprintf(out, out_size, "<%s, \"%s\">",
                     token_type_name(t->type), t->lexeme);
    }

    return (n < 0) ? 0 : n;
}

void token_print(const Token *t, FILE *stream)
{
    char buffer[MAX_LEXEME_LEN + MAX_ERRMSG_LEN + 64];
    if (stream == NULL) stream = stdout;
    token_format(t, buffer, sizeof buffer);
    fputs(buffer, stream);
}

void token_print_header(FILE *stream)
{
    if (stream == NULL) stream = stdout;
    fprintf(stream, "%-6s  %-44s  %s\n", "LINHA", "TOKEN", "LEXEMA");
    fprintf(stream,
        "------  --------------------------------------------  "
        "------------------------------\n");
}

void token_print_row(const Token *t, FILE *stream)
{
    char buffer[MAX_LEXEME_LEN + MAX_ERRMSG_LEN + 64];
    if (stream == NULL) stream = stdout;
    token_format(t, buffer, sizeof buffer);
    fprintf(stream, "%-6d  %-44s  %s\n", t->line, buffer,
            (t->type == TOK_EOF) ? "" : t->lexeme);
}
