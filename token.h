/* =========================================================================
 * token.h
 * -------------------------------------------------------------------------
 * Definicao do conjunto de tokens da linguagem, da estrutura Token e das
 * rotinas de formatacao/impressao no formato <NOME_TOKEN, VALOR_ATRIBUTO>.
 *
 * Projeto : Compilador - Etapa 1 (Analisador Lexico e Tabela de Simbolos)
 * Disciplina: Compiladores - CEFET-MG
 * Padrao  : C99 (compila tambem como C89 com pequenos ajustes)
 * ========================================================================= */
#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

/* Tamanho maximo de um lexema (literais longos sao truncados com aviso). */
#define MAX_LEXEME_LEN 256

/* Tamanho maximo da mensagem associada a um token de erro. */
#define MAX_ERRMSG_LEN 160

/* Declaracao incompleta: evita dependencia circular com symbol_table.h.
 * A definicao completa de struct Symbol esta em symbol_table.h. */
struct Symbol;

/* -------------------------------------------------------------------------
 * TokenType - categorias lexicas reconhecidas pelo AFD.
 *
 * A ordem NAO e arbitraria: as palavras reservadas ocupam uma faixa
 * contigua (TOK_PROGRAM .. TOK_WRITE) para que o teste "e palavra
 * reservada?" seja uma simples comparacao de intervalo. Ver
 * token_is_reserved().
 * ------------------------------------------------------------------------- */
typedef enum {
    /* ----- identificador ------------------------------------------------ */
    TOK_ID = 0,

    /* ----- palavras reservadas (faixa contigua) -------------------------- */
    TOK_PROGRAM,        /* program */
    TOK_BEGIN,          /* begin   */
    TOK_END,            /* end     */
    TOK_INT,            /* int     */
    TOK_FLOAT,          /* float   */
    TOK_CHAR,           /* char    */
    TOK_IF,             /* if      */
    TOK_THEN,           /* then    */
    TOK_ELSE,           /* else    */
    TOK_REPEAT,         /* repeat  */
    TOK_UNTIL,          /* until   */
    TOK_WHILE,          /* while   */
    TOK_DO,             /* do      */
    TOK_READ,           /* read    */
    TOK_WRITE,          /* write   */

    /* ----- constantes ---------------------------------------------------- */
    TOK_INT_CONST,      /* digit+                  ex.: 42        */
    TOK_FLOAT_CONST,    /* digit+ "." digit+       ex.: 3.14      */
    TOK_CHAR_CONST,     /* "'" carac "'"           ex.: 'N'       */
    TOK_LITERAL,        /* '"' caractere* '"'      ex.: "texto"   */

    /* ----- operadores relacionais (relop) -------------------------------- */
    TOK_EQ,             /* ==  */
    TOK_NE,             /* !=  */
    TOK_GT,             /* >   */
    TOK_GE,             /* >=  */
    TOK_LT,             /* <   */
    TOK_LE,             /* <=  */

    /* ----- operadores aditivos (addop) ----------------------------------- */
    TOK_PLUS,           /* +   */
    TOK_MINUS,          /* -   */
    TOK_OR,             /* ||  */

    /* ----- operadores multiplicativos (mulop) ---------------------------- */
    TOK_MUL,            /* *   */
    TOK_DIV,            /* /   */
    TOK_MOD,            /* %   */
    TOK_AND,            /* &&  */

    /* ----- operador unario e atribuicao ---------------------------------- */
    TOK_NOT,            /* !   (fator-a ::= "!" factor) */
    TOK_ASSIGN,         /* =   (assign-stmt e decl)     */

    /* ----- delimitadores -------------------------------------------------- */
    TOK_SEMICOLON,      /* ;   */
    TOK_COMMA,          /* ,   */
    TOK_LPAREN,         /* (   */
    TOK_RPAREN,         /* )   */
    TOK_DOT,            /* .   */
    TOK_COLON,          /* :   (ver LEX_ACCEPT_COLON em lexer.c) */

    /* ----- controle -------------------------------------------------------- */
    TOK_EOF,            /* fim de arquivo             */
    TOK_ERROR,          /* erro lexico (ver .message) */

    TOK_TYPE_COUNT      /* sentinela: numero de categorias */
} TokenType;

/* Limites da faixa de palavras reservadas. */
#define TOK_RESERVED_FIRST TOK_PROGRAM
#define TOK_RESERVED_LAST  TOK_WRITE

/* -------------------------------------------------------------------------
 * Token - unidade lexica devolvida por get_next_token().
 *
 * O campo 'symbol' aponta para a entrada da Tabela de Simbolos quando o
 * token e um identificador ou uma palavra reservada; caso contrario e NULL.
 * 'symbol_index' replica o indice dessa entrada (ou -1), permitindo imprimir
 * o atributo do token sem precisar desreferenciar o ponteiro.
 * ------------------------------------------------------------------------- */
typedef struct {
    TokenType       type;                      /* categoria lexica           */
    char            lexeme[MAX_LEXEME_LEN];    /* texto reconhecido          */
    int             line;                      /* linha de inicio (1-based)  */
    int             column;                    /* coluna de inicio (1-based) */
    int             symbol_index;              /* indice na TS, ou -1        */
    struct Symbol  *symbol;                    /* entrada na TS, ou NULL     */
    char            message[MAX_ERRMSG_LEN];   /* texto do erro (TOK_ERROR)  */
} Token;

/* -------------------------------------------------------------------------
 * Operacoes sobre Token
 * ------------------------------------------------------------------------- */

/* Zera todos os campos do token (type = TOK_ERROR, symbol_index = -1). */
void token_clear(Token *t);

/* Nome simbolico da categoria, ex.: TOK_FLOAT_CONST -> "FLOAT_CONST". */
const char *token_type_name(TokenType type);

/* 1 se 'type' esta na faixa das palavras reservadas, 0 caso contrario. */
int token_is_reserved(TokenType type);

/* 1 se o token carrega um valor constante (int/float/char/literal). */
int token_is_constant(TokenType type);

/* Escreve a representacao <NOME_TOKEN, VALOR_ATRIBUTO> em 'out'.
 * Retorna o numero de caracteres escritos (excluindo o '\0'). */
int token_format(const Token *t, char *out, size_t out_size);

/* Imprime <NOME_TOKEN, VALOR_ATRIBUTO> em 'stream' (sem quebra de linha). */
void token_print(const Token *t, FILE *stream);

/* Imprime uma linha tabular: linha | <NOME, ATRIBUTO> | lexema. */
void token_print_row(const Token *t, FILE *stream);

/* Cabecalho da tabela usada por token_print_row(). */
void token_print_header(FILE *stream);

#endif /* TOKEN_H */
