/* =========================================================================
 * lexer.c - Analisador Lexico (Automato Finito Deterministico).
 * -------------------------------------------------------------------------
 * Esboco do automato (q0 = S_START):
 *
 *   q0 --letra|_--> S_ID --(letra|digito|_)*--> [ID / palavra reservada]
 *   q0 --digito--> S_INT --digito*--> [INT_CONST]
 *                    |  '.'
 *                    +--> S_FLOAT_DOT --digito--> S_FLOAT --digito*--> [FLOAT_CONST]
 *   q0 --'"'--> S_LITERAL --caractere*--> '"' --> [LITERAL]
 *   q0 --'\''--> S_CHAR_BODY --carac--> S_CHAR_CLOSE --'\''--> [CHAR_CONST]
 *   q0 --'='--> S_EQUAL   --'='--> [EQ]      | outro --> [ASSIGN]
 *   q0 --'<'--> S_LESS    --'='--> [LE]      | outro --> [LT]
 *   q0 --'>'--> S_GREATER --'='--> [GE]      | outro --> [GT]
 *   q0 --'!'--> S_BANG    --'='--> [NE]      | outro --> [NOT]
 *   q0 --'&'--> S_AMP     --'&'--> [AND]     | outro --> erro
 *   q0 --'|'--> S_PIPE    --'|'--> [OR]      | outro --> erro
 *   q0 --'{'--> S_BRACE   --'*'--> S_COMMENT ... S_COMMENT_STAR --'}'--> q0
 *   q0 --+-*%/;,().:--> [token de um caractere]
 *
 * Estados finais que consomem um caractere a mais (S_EQUAL, S_LESS, ...)
 * fazem lookahead com lx_peek() e so avancam quando o casamento ocorre -
 * ou seja, o "retract" classico e implicito.
 * ========================================================================= */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lexer.h"

/* Sentinela de fim de arquivo (fora da faixa de unsigned char). */
#define LEX_EOF (-1)

/* -------------------------------------------------------------------------
 * LEX_ACCEPT_COLON - tratamento do caractere ':'
 *
 * A gramatica do enunciado escreve  decl ::= ident-list "=" type , e o ':'
 * nao aparece em nenhuma producao. Porem TODOS os programas de teste do
 * enunciado usam ':' como separador de declaracao ("base, altura : float;")
 * e apontam como errados outros trechos da mesma linha ("1a is float"),
 * o que indica que o ':' e a forma pretendida.
 *
 *   1 (padrao) -> ':' e reconhecido como o token COLON; cabe ao analisador
 *                 sintatico (Etapa 2) aceita-lo em decl. Com isso "val := 34"
 *                 produz COLON seguido de ASSIGN - erro sintatico, nao lexico.
 *   0          -> leitura estrita da gramatica: ':' e simbolo invalido e
 *                 gera erro lexico.
 * ------------------------------------------------------------------------- */
#ifndef LEX_ACCEPT_COLON
#define LEX_ACCEPT_COLON 1
#endif

/* -------------------------------------------------------------------------
 * Estados do AFD
 * ------------------------------------------------------------------------- */
typedef enum {
    S_START = 0,     /* q0 - estado inicial                                */
    S_ID,            /* lendo identificador / palavra reservada            */
    S_INT,           /* lendo digitos da parte inteira                     */
    S_FLOAT_DOT,     /* leu "digito+ ." e exige ao menos um digito         */
    S_FLOAT,         /* lendo digitos da parte fracionaria                 */
    S_LITERAL,       /* dentro de uma string entre aspas duplas            */
    S_CHAR_BODY,     /* leu a aspa simples de abertura                     */
    S_CHAR_CLOSE,    /* leu o caractere, espera a aspa de fechamento       */
    S_EQUAL,         /* leu '='                                            */
    S_LESS,          /* leu '<'                                            */
    S_GREATER,       /* leu '>'                                            */
    S_BANG,          /* leu '!'                                            */
    S_AMP,           /* leu '&'                                            */
    S_PIPE,          /* leu '|'                                            */
    S_BRACE,         /* leu '{' - pode iniciar comentario                  */
    S_COMMENT,       /* dentro de {* ... *}                                */
    S_COMMENT_STAR   /* dentro do comentario, leu '*'                      */
} LexState;

/* -------------------------------------------------------------------------
 * Classificadores do alfabeto.
 *
 * Nao usamos <ctype.h> de proposito: isalpha() depende de locale e pode
 * aceitar bytes acima de 127, enquanto a gramatica define explicitamente
 * letter ::= [A-Za-z] e digit ::= [0-9].
 * ------------------------------------------------------------------------- */
static int is_letter(int c)   { return (c >= 'a' && c <= 'z') ||
                                       (c >= 'A' && c <= 'Z'); }
static int is_digit(int c)    { return  c >= '0' && c <= '9'; }
static int is_id_start(int c) { return is_letter(c) || c == '_'; }
static int is_id_part(int c)  { return is_letter(c) || is_digit(c) || c == '_'; }
static int is_blank(int c)    { return c == ' '  || c == '\t' || c == '\r' ||
                                       c == '\n' || c == '\f' || c == '\v'; }

/* =========================================================================
 * Navegacao no buffer do fonte
 * ========================================================================= */

/* Caractere corrente, sem consumir. */
static int lx_peek(const Lexer *lx)
{
    if (lx->pos >= lx->length) return LEX_EOF;
    return (int)(unsigned char)lx->source[lx->pos];
}

/* Consome e devolve o caractere corrente, atualizando linha e coluna. */
static int lx_advance(Lexer *lx)
{
    int c = lx_peek(lx);
    if (c == LEX_EOF) return LEX_EOF;

    lx->pos++;
    if (c == '\n') { lx->line++; lx->column = 1; }
    else           { lx->column++; }
    return c;
}

/* =========================================================================
 * Construcao de tokens
 * ========================================================================= */

/* Acrescenta um caractere ao lexema em construcao, com protecao de limite. */
static void buf_push(char *buf, int *len, int c)
{
    if (*len < MAX_LEXEME_LEN - 1) buf[(*len)++] = (char)c;
}

/* Monta um token comum (nao-erro). */
static Token make_token(Lexer *lx, TokenType type, const char *lexeme,
                        int line, int column)
{
    Token t;

    token_clear(&t);
    t.type   = type;
    t.line   = line;
    t.column = column;

    if (lexeme != NULL) {
        strncpy(t.lexeme, lexeme, MAX_LEXEME_LEN - 1);
        t.lexeme[MAX_LEXEME_LEN - 1] = '\0';
    }

    lx->token_count++;
    return t;
}

/* Monta um token de erro, contabiliza e (opcionalmente) reporta na hora.
 *
 * O lexer NAO aborta: devolve TOK_ERROR e segue a varredura. Isso permite
 * que uma unica execucao liste todos os erros do programa fonte, conforme
 * exigido pelo relatorio. Quem quiser parar no primeiro erro basta
 * interromper o laco de chamada ao receber TOK_ERROR. */
static Token make_error(Lexer *lx, int line, int column,
                        const char *lexeme, const char *fmt, ...)
{
    Token   t;
    va_list ap;
    FILE   *es;

    token_clear(&t);
    t.type   = TOK_ERROR;
    t.line   = line;
    t.column = column;

    if (lexeme != NULL) {
        strncpy(t.lexeme, lexeme, MAX_LEXEME_LEN - 1);
        t.lexeme[MAX_LEXEME_LEN - 1] = '\0';
    }

    va_start(ap, fmt);
    vsnprintf(t.message, MAX_ERRMSG_LEN, fmt, ap);
    va_end(ap);

    lx->error_count++;

    if (lx->report_errors) {
        es = (lx->error_stream != NULL) ? lx->error_stream : stderr;
        fprintf(es, "%s:%d:%d: erro lexico: %s\n",
                (lx->filename != NULL) ? lx->filename : "<fonte>",
                line, column, t.message);
    }
    return t;
}

/* Representacao legivel de um byte para mensagens de erro. */
static void describe_char(int c, char *out, size_t n)
{
    if (c >= 33 && c <= 126)      snprintf(out, n, "'%c'", c);
    else if (c == ' ')            snprintf(out, n, "espaco");
    else                          snprintf(out, n, "0x%02X", (unsigned)c & 0xFF);
}

/* =========================================================================
 * Ciclo de vida
 * ========================================================================= */

static void lexer_reset(Lexer *lx)
{
    lx->source        = NULL;
    lx->length        = 0;
    lx->pos           = 0;
    lx->line          = 1;
    lx->column        = 1;
    lx->error_count   = 0;
    lx->token_count   = 0;
    lx->filename      = NULL;
    lx->symbols       = NULL;
    lx->report_errors = 1;
    lx->error_stream  = NULL;
}

int lexer_init_file(Lexer *lx, const char *filename, SymbolTable *st)
{
    FILE  *fp;
    long   size;
    size_t read;

    if (lx == NULL || filename == NULL) return 0;
    lexer_reset(lx);

    /* "rb" preserva o \r de arquivos CRLF; ele e tratado como espaco. */
    fp = fopen(filename, "rb");
    if (fp == NULL) return 0;

    if (fseek(fp, 0L, SEEK_END) != 0) { fclose(fp); return 0; }
    size = ftell(fp);
    if (size < 0) { fclose(fp); return 0; }
    rewind(fp);

    lx->source = (char *)malloc((size_t)size + 1);
    if (lx->source == NULL) { fclose(fp); return 0; }

    read = fread(lx->source, 1, (size_t)size, fp);
    lx->source[read] = '\0';
    lx->length       = read;
    fclose(fp);

    lx->filename = filename;
    lx->symbols  = st;
    return 1;
}

int lexer_init_string(Lexer *lx, const char *source, SymbolTable *st)
{
    size_t len;

    if (lx == NULL || source == NULL) return 0;
    lexer_reset(lx);

    len = strlen(source);
    lx->source = (char *)malloc(len + 1);
    if (lx->source == NULL) return 0;

    memcpy(lx->source, source, len + 1);
    lx->length   = len;
    lx->filename = "<string>";
    lx->symbols  = st;
    return 1;
}

void lexer_free(Lexer *lx)
{
    if (lx == NULL) return;
    free(lx->source);
    lexer_reset(lx);
}

int lexer_error_count(const Lexer *lx) { return (lx != NULL) ? lx->error_count : 0; }
int lexer_token_count(const Lexer *lx) { return (lx != NULL) ? lx->token_count : 0; }

/* =========================================================================
 * get_next_token - o laco de transicao do AFD
 * ========================================================================= */
Token get_next_token(Lexer *lx)
{
    char     buf[MAX_LEXEME_LEN];
    char     desc[32];
    LexState state   = S_START;
    int      len     = 0;
    int      line    = 1;
    int      column  = 1;
    int      cmt_line = 0;   /* linha de abertura do comentario corrente */
    int      cmt_col  = 0;
    int      c;

    if (lx == NULL) {
        Token t;
        token_clear(&t);
        t.type = TOK_EOF;
        return t;
    }

    for (;;) {
        c = lx_peek(lx);

        switch (state) {

        /* ------------------------------------------------------------------
         * q0 - estado inicial: descarta brancos e decide a categoria.
         * ------------------------------------------------------------------ */
        case S_START:
            line   = lx->line;
            column = lx->column;
            len    = 0;

            if (c == LEX_EOF)
                return make_token(lx, TOK_EOF, "", line, column);

            if (is_blank(c)) { lx_advance(lx); break; }

            if (is_id_start(c)) {
                buf_push(buf, &len, lx_advance(lx));
                state = S_ID;
                break;
            }

            if (is_digit(c)) {
                buf_push(buf, &len, lx_advance(lx));
                state = S_INT;
                break;
            }

            switch (c) {
            /* --- inicios de estados compostos --- */
            case '{':
                cmt_line = lx->line; cmt_col = lx->column;
                lx_advance(lx); state = S_BRACE;   break;
            case '"':  lx_advance(lx); state = S_LITERAL;   break;
            case '\'': lx_advance(lx); state = S_CHAR_BODY; break;
            case '=':  lx_advance(lx); state = S_EQUAL;     break;
            case '<':  lx_advance(lx); state = S_LESS;      break;
            case '>':  lx_advance(lx); state = S_GREATER;   break;
            case '!':  lx_advance(lx); state = S_BANG;      break;
            case '&':  lx_advance(lx); state = S_AMP;       break;
            case '|':  lx_advance(lx); state = S_PIPE;      break;

            /* --- tokens de um unico caractere (estados finais diretos) --- */
            case '+': lx_advance(lx); return make_token(lx, TOK_PLUS,      "+", line, column);
            case '-': lx_advance(lx); return make_token(lx, TOK_MINUS,     "-", line, column);
            case '*': lx_advance(lx); return make_token(lx, TOK_MUL,       "*", line, column);
            case '/': lx_advance(lx); return make_token(lx, TOK_DIV,       "/", line, column);
            case '%': lx_advance(lx); return make_token(lx, TOK_MOD,       "%", line, column);
            case ';': lx_advance(lx); return make_token(lx, TOK_SEMICOLON, ";", line, column);
            case ',': lx_advance(lx); return make_token(lx, TOK_COMMA,     ",", line, column);
            case '(': lx_advance(lx); return make_token(lx, TOK_LPAREN,    "(", line, column);
            case ')': lx_advance(lx); return make_token(lx, TOK_RPAREN,    ")", line, column);
            case '.': lx_advance(lx); return make_token(lx, TOK_DOT,       ".", line, column);
#if LEX_ACCEPT_COLON
            case ':': lx_advance(lx); return make_token(lx, TOK_COLON,     ":", line, column);
#endif

            default:
                /* Byte fora do alfabeto da linguagem. */
                if (c >= 0x80) {
                    /* Sequencia UTF-8 (ex.: acento em "pontuacao"): consome o
                     * byte lider e os de continuacao, para emitir um unico
                     * erro em vez de um por byte. */
                    buf_push(buf, &len, lx_advance(lx));
                    while ((c = lx_peek(lx)) != LEX_EOF && (c & 0xC0) == 0x80)
                        buf_push(buf, &len, lx_advance(lx));
                    buf[len] = '\0';
                    return make_error(lx, line, column, buf,
                        "caractere nao-ASCII \"%s\" nao pertence ao alfabeto "
                        "da linguagem", buf);
                }
                lx_advance(lx);
                describe_char(c, desc, sizeof desc);
                buf[0] = (char)c; buf[1] = '\0';
                return make_error(lx, line, column, buf,
                    "simbolo invalido %s", desc);
            }
            break;

        /* ------------------------------------------------------------------
         * Identificadores e palavras reservadas.
         * identifier ::= (letter | "_") (letter | digit | "_")*
         * ------------------------------------------------------------------ */
        case S_ID:
            if (is_id_part(c)) { buf_push(buf, &len, lx_advance(lx)); break; }

            buf[len] = '\0';
            {
                Token   t;
                Symbol *sym = symtab_install(lx->symbols, buf, line);

                if (sym == NULL)
                    return make_error(lx, line, column, buf,
                        "falha ao instalar \"%s\" na tabela de simbolos", buf);

                /* A tabela ja distingue reservada de identificador: basta
                 * copiar o tipo registrado na entrada. */
                t = make_token(lx, sym->type, buf, line, column);
                t.symbol       = sym;
                t.symbol_index = sym->index;
                return t;
            }

        /* ------------------------------------------------------------------
         * Constantes numericas.
         * integer_const ::= digit+        float_const ::= digit+ "." digit+
         * ------------------------------------------------------------------ */
        case S_INT:
            if (is_digit(c)) { buf_push(buf, &len, lx_advance(lx)); break; }

            if (c == '.') { buf_push(buf, &len, lx_advance(lx)); state = S_FLOAT_DOT; break; }

            if (is_id_start(c)) {
                /* Ex.: "1a", "1c" - um identificador nao pode comecar com
                 * digito. Consome o resto do lexema para nao gerar uma
                 * cascata de erros. */
                while (is_id_part(lx_peek(lx))) buf_push(buf, &len, lx_advance(lx));
                buf[len] = '\0';
                return make_error(lx, line, column, buf,
                    "lexema mal formado \"%s\": identificador nao pode "
                    "comecar com digito", buf);
            }

            buf[len] = '\0';
            return make_token(lx, TOK_INT_CONST, buf, line, column);

        case S_FLOAT_DOT:
            if (is_digit(c)) { buf_push(buf, &len, lx_advance(lx)); state = S_FLOAT; break; }

            /* Ex.: "34." - a gramatica exige digito+ depois do ponto. */
            buf[len] = '\0';
            return make_error(lx, line, column, buf,
                "constante float mal formada \"%s\": e necessario ao menos "
                "um digito apos o ponto", buf);

        case S_FLOAT:
            if (is_digit(c)) { buf_push(buf, &len, lx_advance(lx)); break; }

            if (c == '.') {
                /* Ex.: "1.2.3" */
                buf_push(buf, &len, lx_advance(lx));
                while (is_digit(lx_peek(lx)) || lx_peek(lx) == '.')
                    buf_push(buf, &len, lx_advance(lx));
                buf[len] = '\0';
                return make_error(lx, line, column, buf,
                    "constante float mal formada \"%s\": ponto decimal "
                    "duplicado", buf);
            }

            if (is_id_start(c)) {
                while (is_id_part(lx_peek(lx))) buf_push(buf, &len, lx_advance(lx));
                buf[len] = '\0';
                return make_error(lx, line, column, buf,
                    "lexema mal formado \"%s\": letra apos constante "
                    "numerica", buf);
            }

            buf[len] = '\0';
            return make_token(lx, TOK_FLOAT_CONST, buf, line, column);

        /* ------------------------------------------------------------------
         * Literal: '"' caractere* '"'
         * caractere = ASCII exceto quebra de linha e aspas.
         * ------------------------------------------------------------------ */
        case S_LITERAL:
            if (c == '"') {
                lx_advance(lx);
                buf[len] = '\0';
                return make_token(lx, TOK_LITERAL, buf, line, column);
            }
            if (c == LEX_EOF) {
                buf[len] = '\0';
                return make_error(lx, line, column, buf,
                    "literal iniciado na linha %d nao foi fechado antes do "
                    "fim do arquivo", line);
            }
            if (c == '\n') {
                /* Nao consome o '\n': ele volta a contar como separador. */
                buf[len] = '\0';
                return make_error(lx, line, column, buf,
                    "literal nao fechado na linha %d (aspas duplas nao podem "
                    "cruzar a quebra de linha)", line);
            }
            buf_push(buf, &len, lx_advance(lx));
            break;

        /* ------------------------------------------------------------------
         * Constante caractere: "'" carac "'"
         * ------------------------------------------------------------------ */
        case S_CHAR_BODY:
            if (c == LEX_EOF || c == '\n')
                return make_error(lx, line, column, "",
                    "constante caractere nao fechada na linha %d", line);

            buf_push(buf, &len, lx_advance(lx));
            state = S_CHAR_CLOSE;
            break;

        case S_CHAR_CLOSE:
            buf[len] = '\0';
            if (c == '\'') {
                lx_advance(lx);
                return make_token(lx, TOK_CHAR_CONST, buf, line, column);
            }
            return make_error(lx, line, column, buf,
                "constante caractere mal formada: esperada aspa simples de "
                "fechamento apos '%s'", buf);

        /* ------------------------------------------------------------------
         * Operadores com lookahead de um caractere.
         * ------------------------------------------------------------------ */
        case S_EQUAL:
            if (c == '=') { lx_advance(lx); return make_token(lx, TOK_EQ, "==", line, column); }
            return make_token(lx, TOK_ASSIGN, "=", line, column);

        case S_LESS:
            if (c == '=') { lx_advance(lx); return make_token(lx, TOK_LE, "<=", line, column); }
            return make_token(lx, TOK_LT, "<", line, column);

        case S_GREATER:
            if (c == '=') { lx_advance(lx); return make_token(lx, TOK_GE, ">=", line, column); }
            return make_token(lx, TOK_GT, ">", line, column);

        case S_BANG:
            if (c == '=') { lx_advance(lx); return make_token(lx, TOK_NE, "!=", line, column); }
            return make_token(lx, TOK_NOT, "!", line, column);   /* fator-a ::= ! factor */

        case S_AMP:
            if (c == '&') { lx_advance(lx); return make_token(lx, TOK_AND, "&&", line, column); }
            return make_error(lx, line, column, "&",
                "simbolo invalido '&': o operador logico e \"&&\"");

        case S_PIPE:
            if (c == '|') { lx_advance(lx); return make_token(lx, TOK_OR, "||", line, column); }
            return make_error(lx, line, column, "|",
                "simbolo invalido '|': o operador logico e \"||\"");

        /* ------------------------------------------------------------------
         * Comentarios: {* ... *}  (podem ocupar varias linhas)
         * ------------------------------------------------------------------ */
        case S_BRACE:
            if (c == '*') { lx_advance(lx); state = S_COMMENT; break; }
            return make_error(lx, cmt_line, cmt_col, "{",
                "simbolo invalido '{': um comentario deve iniciar com \"{*\"");

        case S_COMMENT:
            if (c == LEX_EOF)
                return make_error(lx, cmt_line, cmt_col, "",
                    "comentario iniciado na linha %d nao foi fechado "
                    "(esperado \"*}\")", cmt_line);
            if (c == '*') { lx_advance(lx); state = S_COMMENT_STAR; break; }
            lx_advance(lx);                 /* qualquer outro caractere      */
            break;                          /* (lx_advance conta as linhas)  */

        case S_COMMENT_STAR:
            if (c == LEX_EOF)
                return make_error(lx, cmt_line, cmt_col, "",
                    "comentario iniciado na linha %d nao foi fechado "
                    "(esperado \"*}\")", cmt_line);
            if (c == '}') { lx_advance(lx); state = S_START;       break; }
            if (c == '*') { lx_advance(lx); /* continua em S_COMMENT_STAR */ break; }
            lx_advance(lx); state = S_COMMENT;
            break;

        default:
            /* Inalcancavel: todos os estados estao tratados acima. */
            return make_error(lx, lx->line, lx->column, "",
                "estado interno invalido no analisador lexico");
        }
    }
}
