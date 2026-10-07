/* =========================================================================
 * main.c - driver do compilador (Etapa 1: Analisador Lexico).
 * -------------------------------------------------------------------------
 * Fluxo da Etapa 1, conforme o enunciado:
 *
 *   1. recebe o caminho do programa fonte pela linha de comando;
 *   2. varre o fonte token a token ate o EOF, imprimindo a sequencia
 *      reconhecida no formato <NOME_TOKEN, "lexema"> ou <NOME_TOKEN, indice_TS>;
 *   3. exibe o conteudo completo da Tabela de Simbolos (palavras reservadas
 *      e identificadores instalados, com seus indices);
 *   4. para cada erro lexico, mostra a mensagem, a linha e a coluna exatas,
 *      alem da propria linha do fonte com um marcador sob o ponto do erro.
 *
 * Sobre erro: o enunciado permite encerrar a compilacao no primeiro erro
 * ("nao e necessario implementar recuperacao de erro"), mas exige que o
 * relatorio apresente todos os erros do programa. Por isso o padrao aqui e
 * listar todos numa unica execucao; a opcao -p reproduz o comportamento de
 * encerrar no primeiro, util para montar o relatorio erro a erro.
 *
 * Uso:
 *   compilador <arquivo-fonte> [opcoes]
 * ========================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "symbol_table.h"
#include "lexer.h"

/* Limite de erros detalhados guardados para o relatorio final. */
#define MAX_REPORTED_ERRORS 256

/* Largura das reguas de secao. */
#define RULE_WIDTH 79

/* -------------------------------------------------------------------------
 * Apresentacao
 * ------------------------------------------------------------------------- */
static void usage(const char *prog)
{
    printf("Compilador - Etapa 1: Analisador Lexico e Tabela de Simbolos\n\n");
    printf("Uso: %s <arquivo-fonte> [opcoes]\n\n", prog);
    printf("Opcoes:\n");
    printf("  -p, --parar-no-erro     encerra a analise no primeiro erro lexico\n");
    printf("                          (padrao: percorre o fonte inteiro e lista\n");
    printf("                           todos os erros encontrados)\n");
    printf("  -i, --identificadores   na Tabela de Simbolos, omite as palavras\n");
    printf("                          reservadas e lista so os identificadores\n");
    printf("  -t, --tokens            exibe apenas a sequencia de tokens\n");
    printf("  -s, --simbolos          exibe apenas a Tabela de Simbolos\n");
    printf("  -h, --help              exibe esta ajuda\n\n");
    printf("Codigo de saida: 0 = sucesso, 1 = erros lexicos, 2 = uso incorreto,\n");
    printf("                 3 = falha ao abrir o arquivo ou falta de memoria.\n");
}

static void print_rule(const char *title)
{
    int i;
    putchar('\n');
    for (i = 0; i < RULE_WIDTH; i++) putchar('=');
    printf("\n %s\n", title);
    for (i = 0; i < RULE_WIDTH; i++) putchar('=');
    putchar('\n');
}

/* -------------------------------------------------------------------------
 * print_source_line - ecoa a linha 'line' do fonte e posiciona um '^' sob a
 * coluna onde o erro foi detectado.
 *
 * Le direto do buffer do lexer (lx->source), que contem o arquivo inteiro.
 * Tabulacoes sao expandidas para um espaco, de modo que o marcador continue
 * alinhado com o texto exibido.
 * ------------------------------------------------------------------------- */
static void print_source_line(const Lexer *lx, int line, int column)
{
    size_t i      = 0;
    int    cur    = 1;
    int    col    = 1;

    if (lx == NULL || lx->source == NULL || line < 1) return;

    /* Posiciona 'i' no primeiro caractere da linha desejada. */
    while (i < lx->length && cur < line) {
        if (lx->source[i] == '\n') cur++;
        i++;
    }
    if (cur != line) return;   /* linha inexistente (ex.: erro no EOF) */

    printf("   %5d | ", line);
    for (; i < lx->length; i++) {
        char c = lx->source[i];
        if (c == '\n' || c == '\r') break;
        putchar((c == '\t') ? ' ' : c);
    }
    printf("\n         | ");
    for (col = 1; col < column; col++) putchar(' ');
    printf("^\n");
}

/* Bloco detalhado de um erro: mensagem, posicao e trecho do fonte. */
static void print_error_detail(const Lexer *lx, const Token *err, int n)
{
    printf("\n[%d] Linha %d, coluna %d: %s\n",
           n, err->line, err->column, err->message);
    print_source_line(lx, err->line, err->column);
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(int argc, char **argv)
{
    SymbolTable  symbols;
    Lexer        lexer;
    Token        token;
    Token        errors[MAX_REPORTED_ERRORS];
    const char  *filename   = NULL;
    int          stop_first = 0;   /* -p: encerrar no primeiro erro        */
    int          only_ids   = 0;   /* -i: so identificadores na TS         */
    int          only_tok   = 0;   /* -t: so a sequencia de tokens         */
    int          only_sym   = 0;   /* -s: so a Tabela de Simbolos          */
    int          nerrors    = 0;   /* erros guardados em 'errors'          */
    int          nvalid     = 0;   /* tokens validos (exclui EOF e erros)  */
    int          aborted    = 0;   /* 1 se a varredura parou por -p        */
    int          status;
    int          i;

    /* ------------------------- linha de comando ------------------------- */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--parar-no-erro")) {
                stop_first = 1;
            } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--identificadores")) {
                only_ids = 1;
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tokens")) {
                only_tok = 1;
            } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--simbolos")) {
                only_sym = 1;
            } else {
                fprintf(stderr, "Opcao desconhecida: %s\n\n", argv[i]);
                usage(argv[0]);
                return 2;
            }
        } else if (filename == NULL) {
            filename = argv[i];
        } else {
            fprintf(stderr, "Erro: informe apenas um arquivo fonte por vez.\n");
            return 2;
        }
    }

    if (filename == NULL) {
        fprintf(stderr, "Erro: arquivo fonte nao informado.\n\n");
        usage(argv[0]);
        return 2;
    }

    /* --------------------------- inicializacao -------------------------- */
    if (!symtab_init(&symbols)) {
        fprintf(stderr, "Erro: memoria insuficiente para a Tabela de Simbolos.\n");
        return 3;
    }

    if (!lexer_init_file(&lexer, filename, &symbols)) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo \"%s\".\n", filename);
        symtab_free(&symbols);
        return 3;
    }

    /* Os erros sao acumulados e detalhados em bloco proprio ao final; o
     * lexer nao deve imprimi-los soltos no meio da tabela de tokens. */
    lexer.report_errors = 0;

    printf("Compilador - Etapa 1 (Analisador Lexico e Tabela de Simbolos)\n");
    printf("Arquivo fonte: %s\n", filename);
    if (stop_first)
        printf("Modo: encerrar no primeiro erro lexico (-p)\n");

    /* ----------------------------- varredura ---------------------------- */
    if (!only_sym) {
        print_rule("SEQUENCIA DE TOKENS");
        token_print_header(stdout);
    }

    for (;;) {
        token = get_next_token(&lexer);

        if (token.type == TOK_ERROR) {
            if (nerrors < MAX_REPORTED_ERRORS) errors[nerrors++] = token;
        } else if (token.type != TOK_EOF) {
            nvalid++;
        }

        if (!only_sym) token_print_row(&token, stdout);

        if (token.type == TOK_EOF) break;

        if (token.type == TOK_ERROR && stop_first) {
            aborted = 1;
            break;
        }
    }

    /* ------------------------------- erros ------------------------------ */
    if (nerrors > 0) {
        print_rule("ERROS LEXICOS");
        for (i = 0; i < nerrors; i++)
            print_error_detail(&lexer, &errors[i], i + 1);

        if (lexer.error_count > nerrors)
            printf("\n... e mais %d erro(s) nao detalhado(s) "
                   "(limite de %d por execucao).\n",
                   lexer.error_count - nerrors, MAX_REPORTED_ERRORS);

        if (aborted)
            printf("\nAnalise encerrada no primeiro erro (-p). Corrija-o e "
                   "execute novamente\npara descobrir os proximos.\n");
        else
            printf("\nObservacao: um unico erro pode mascarar os seguintes "
                   "(um comentario nao\nfechado, por exemplo, consome o resto "
                   "do arquivo). Corrija do primeiro para\no ultimo, "
                   "reexecutando a cada correcao.\n");
    }

    /* ------------------------ tabela de simbolos ------------------------ */
    /* So faz sentido exibir a tabela completa quando a varredura chegou ao
     * fim do arquivo; se foi abortada no primeiro erro, ela estaria
     * incompleta e induziria a conclusoes erradas. */
    if (!only_tok) {
        if (aborted) {
            print_rule("TABELA DE SIMBOLOS");
            printf("Nao exibida: a analise foi interrompida no primeiro erro, "
                   "portanto a\ntabela estaria incompleta. Corrija o erro "
                   "acima e execute novamente.\n");
        } else {
            print_rule(only_ids ? "TABELA DE SIMBOLOS (identificadores)"
                                : "TABELA DE SIMBOLOS");
            if (only_ids) symtab_print_identifiers(&symbols, stdout);
            else          symtab_print(&symbols, stdout);
        }
    }

    /* ------------------------------ resumo ------------------------------ */
    print_rule("RESUMO DA ANALISE LEXICA");
    printf("Tokens reconhecidos : %d\n", nvalid);
    printf("Erros lexicos       : %d\n", lexer.error_count);
    printf("Linhas analisadas   : %d\n", lexer.line);
    printf("Resultado           : %s\n",
           (lexer.error_count == 0)
               ? "SUCESSO - nenhum erro lexico encontrado"
               : "FALHA - foram encontrados erros lexicos");

    status = (lexer.error_count == 0) ? 0 : 1;

    lexer_free(&lexer);
    symtab_free(&symbols);
    return status;
}
