# Compilador, Etapa 1: analisador léxico e tabela de símbolos

Analisador léxico em C99, sem Flex/Lex ou outro gerador, para a linguagem do
enunciado de Compiladores (CEFET-MG, Prof.ª Kecia Marques, 2026/2). A varredura
é feita por um AFD escrito à mão.

Dado um arquivo fonte, o programa imprime:

1. a sequência de tokens;
2. a Tabela de Símbolos (palavras reservadas e identificadores, com índices);
3. os erros léxicos, com linha e coluna.

---

## 1. Compilação

Com `make`:

```bash
make
```

Gera `compilador` (`compilador.exe` no Windows) com `-std=c99 -Wall -Wextra -pedantic -O2`.
Não há avisos.

```bash
make clean                       # remove objetos e executável
make run FILE=testes/teste1.txt  # compila e analisa um arquivo
```

Direto com o GCC:

```bash
gcc -std=c99 -Wall -Wextra -pedantic -O2 -o compilador main.c lexer.c symbol_table.c token.c
```

No Windows sem GCC, o [w64devkit](https://github.com/skeeto/w64devkit/releases)
ou o MinGW-w64 bastam (adicionar `bin` ao `PATH`). MSYS2 e WSL não são necessários.

Com MSVC:

```bat
cl /nologo /W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:compilador.exe main.c lexer.c symbol_table.c token.c
```

`/utf-8` e `/D_CRT_SECURE_NO_WARNINGS` só evitam avisos do MSVC (que trata `fopen`
e `strncpy` como inseguras). O código-fonte é ASCII, então compila igual nos dois
toolchains.

---

## 2. Uso

```
compilador <arquivo-fonte> [opções]
```

```bash
./compilador testes/teste6_sucesso.txt
./compilador testes/teste6_erro.txt -p
./compilador testes/teste2.txt -i
```

### Opções

| Opção | Efeito |
|---|---|
| (nenhuma) | Analisa o arquivo inteiro e lista todos os erros léxicos |
| `-p`, `--parar-no-erro` | Para no primeiro erro léxico |
| `-i`, `--identificadores` | Omite as palavras reservadas na Tabela de Símbolos |
| `-t`, `--tokens` | Imprime só a sequência de tokens |
| `-s`, `--simbolos` | Imprime só a Tabela de Símbolos |
| `-h`, `--help` | Mostra a ajuda |

### Códigos de saída

| Código | Significado |
|---|---|
| 0 | Nenhum erro léxico |
| 1 | Erros léxicos encontrados |
| 2 | Uso incorreto da linha de comando |
| 3 | Falha ao abrir o arquivo ou falta de memória |

### Tratamento de erro

O enunciado dispensa recuperação de erro, mas pede que o relatório apresente os
erros do programa analisado. Por isso há dois modos:

- Padrão: continua após cada erro e lista todos.
- `-p`: para no primeiro erro, para o ciclo do relatório (corrige o fonte, roda
  de novo, documenta o próximo).

Um erro pode mascarar os seguintes. No Teste 1, o comentário `{*` que nunca
fecha consome o resto do arquivo e só um erro é reportado; corrigido esse,
aparecem os demais. O ciclo corrige/roda continua necessário mesmo no modo padrão.

---

## 3. Formato da saída

### Tokens

Cada token sai como `<NOME_TOKEN, VALOR_ATRIBUTO>`:

```
LINHA   TOKEN                                         LEXEMA
------  --------------------------------------------  ------------------------------
2       <PROGRAM, 0>                                  program
2       <ID, 15>                                      testeSeis
3       <ID, 16>                                      x
3       <COLON, ":">                                  :
3       <FLOAT, 4>                                    float
8       <FLOAT_CONST, "10.5">                         10.5
11      <AND, "&&">                                   &&
16      <LITERAL, "Resultado final do calculo:">      Resultado final do calculo:
```

Atributo:

- Identificadores e palavras reservadas: índice da entrada na Tabela de Símbolos
  (`<ID, 15>`, `<PROGRAM, 0>`).
- Demais tokens: o lexema entre aspas (`<PLUS, "+">`, `<FLOAT_CONST, "10.5">`).

### Tabela de Símbolos

```
INDICE  LEXEMA                    TOKEN           CLASSE          LINHA   USOS
------  ------------------------  --------------  --------------  ------  ----
0       program                   PROGRAM         PALAVRA-RESV    2       1
11      while                     WHILE           PALAVRA-RESV    11      1
15      testeSeis                 ID              IDENTIFICADOR   2       1
16      x                         ID              IDENTIFICADOR   3       6
```

As 15 palavras reservadas ocupam os índices 0 a 14; os identificadores começam
em 15. `LINHA` é a primeira ocorrência (`-` se a palavra reservada não aparece no
fonte). `USOS` é o número de vezes que o símbolo foi reconhecido.

### Erros léxicos

Cada erro traz mensagem, posição e a linha do fonte com um marcador na coluna:

```
[1] Linha 4, coluna 9: simbolo invalido '@'
       4 |   opcao @ : int; {* Erro lexico na linha 4: caracter invalido '@' *}
         |         ^
```

---

## 4. Arquitetura

| Arquivo | Conteúdo |
|---|---|
| `token.h` / `token.c` | `enum TokenType` (41 categorias léxicas, EOF e erro léxico), `struct Token` e a formatação `<NOME_TOKEN, VALOR_ATRIBUTO>` |
| `symbol_table.h` / `symbol_table.c` | Tabela hash com encadeamento separado: pré-carga das palavras reservadas, instalação de identificadores, impressão |
| `lexer.h` / `lexer.c` | O AFD. Expõe `Token get_next_token(Lexer *)`, além de inicialização e liberação |
| `main.c` | Linha de comando, laço de varredura e apresentação (tokens, erros, tabela, resumo) |

### `struct Token`

```
type          categoria léxica
lexeme        texto reconhecido
line, column  posição de início (1-based)
symbol_index  índice na Tabela de Símbolos, ou -1
symbol        ponteiro para a entrada na tabela, ou NULL
message       diagnóstico, quando type == TOK_ERROR
```

`token.h` só declara `struct Symbol` (tipo incompleto) para evitar dependência
circular com `symbol_table.h`.

### Analisador léxico

O arquivo é lido por inteiro para memória; lookahead e retract são só
incremento e decremento do índice de leitura. `get_next_token()` é um laço de
transição de estados que termina ao atingir um estado final e devolve o `Token`.

São 17 estados:

| Estado | Função |
|---|---|
| `S_START` | q0: descarta brancos e escolhe a categoria pelo primeiro caractere |
| `S_ID` | identificador ou palavra reservada |
| `S_INT`, `S_FLOAT_DOT`, `S_FLOAT` | constantes numéricas; `S_FLOAT_DOT` exige ao menos um dígito após o ponto |
| `S_LITERAL` | dentro de `"..."` |
| `S_CHAR_BODY`, `S_CHAR_CLOSE` | dentro de `'c'` |
| `S_EQUAL`, `S_LESS`, `S_GREATER`, `S_BANG` | operadores com segundo caractere opcional (`=`/`==`, `<`/`<=`, ...) |
| `S_AMP`, `S_PIPE` | `&&` e `\|\|`; `&` ou `\|` isolado é erro |
| `S_BRACE`, `S_COMMENT`, `S_COMMENT_STAR` | comentários `{* ... *}`, inclusive multilinha |

Nos estados de segundo caractere opcional, o índice só avança se o casamento
ocorrer, então não há retract explícito.

A classificação de caracteres (`is_letter`, `is_digit`, ...) usa comparações
explícitas e não `<ctype.h>`: `isalpha()` depende de locale e pode aceitar bytes
acima de 127, e a gramática define `letter ::= [A-Za-z]`.

### Tabela de Símbolos

Tabela hash com encadeamento separado, 211 buckets e função djb2. Cada entrada
guarda lexema, tipo, índice, linha da primeira ocorrência e número de usos.

- As palavras reservadas são carregadas em `symtab_init()`, antes da leitura do
  fonte. O lexer chama `symtab_install()` com o lexema e copia `sym->type`;
  não há cadeia de `if` comparando strings.
- Além dos buckets, há uma lista na ordem de inserção. Ela torna a impressão
  determinística e permite liberar cada entrada exatamente uma vez.

A busca diferencia maiúsculas de minúsculas: `write` é palavra reservada;
`Write` e `WRITE` são identificadores distintos.

---

## 5. Tokens

| Categoria | Tokens |
|---|---|
| Identificador | `ID`: `(letter \| "_") (letter \| digit \| "_")*` |
| Palavras reservadas | `program`, `begin`, `end`, `int`, `float`, `char`, `if`, `then`, `else`, `repeat`, `until`, `while`, `do`, `read`, `write` |
| Constantes | `INT_CONST` (`digit+`), `FLOAT_CONST` (`digit+ "." digit+`), `CHAR_CONST` (`'c'`), `LITERAL` (`"..."`) |
| Relacionais | `==` `!=` `>` `>=` `<` `<=` |
| Aditivos | `+` `-` `\|\|` |
| Multiplicativos | `*` `/` `%` `&&` |
| Unário e atribuição | `!` `=` |
| Delimitadores | `;` `,` `(` `)` `.` `:` |

Comentários `{* ... *}` são descartados (podem ter várias linhas), mas a
contagem de linhas continua dentro deles.

---

## 6. Erros léxicos

| Situação | Exemplo | Mensagem |
|---|---|---|
| Símbolo fora do alfabeto | `a @ b` | `simbolo invalido '@'` |
| Caractere não-ASCII | `pontuação` | `caractere nao-ASCII "ç" nao pertence ao alfabeto da linguagem` |
| Comentário não fechado | `{* ...` até o EOF | `comentario iniciado na linha N nao foi fechado (esperado "*}")` |
| `{` sem `*` | `a = {` | `simbolo invalido '{': um comentario deve iniciar com "{*"` |
| Literal cruzando a linha | `write("abc);` | `literal nao fechado na linha N` |
| Literal aberto no EOF | `"abc` + EOF | `literal ... nao foi fechado antes do fim do arquivo` |
| Constante char malformada | `'ab'` | `constante caractere mal formada: esperada aspa simples de fechamento` |
| Float sem dígito após o ponto | `34.` | `constante float mal formada "34.": e necessario ao menos um digito apos o ponto` |
| Ponto decimal duplicado | `1.2.3` | `constante float mal formada "1.2.3": ponto decimal duplicado` |
| Identificador iniciando com dígito | `1a`, `1c` | `lexema mal formado "1a": identificador nao pode comecar com digito` |
| `&` ou `\|` isolado | `a & b` | `simbolo invalido '&': o operador logico e "&&"` |

Em todos os casos o trecho ofensivo é descartado e a análise continua, de modo
que uma execução lista todos os erros.

---

## 7. Decisões de projeto

Duas ambiguidades do enunciado foram resolvidas por decisão explícita. Ambas
alteram a saída dos testes e devem ser confirmadas com a professora.

### 7.1. O caractere `:`

A gramática escreve `decl ::= ident-list "=" type` e `:` não aparece em nenhuma
produção. Todos os programas de teste, porém, usam `:` como separador de
declaração (`base, altura : float;`) e apontam como erros outros trechos da
mesma linha (`area, 1a is float;`). A leitura mais provável é que `:` seja o
pretendido e a produção `decl` esteja inconsistente.

- Padrão: `:` vira o token `COLON`; aceitá-lo em `decl` fica para o analisador
  sintático (Etapa 2). Assim, `val := 34` gera `COLON` seguido de `ASSIGN`, o
  que é erro sintático, não léxico.
- Para seguir a gramática literalmente, compilar com `-DLEX_ACCEPT_COLON=0`; o
  `:` volta a ser símbolo inválido. Ver comentário no início de `lexer.c`.

### 7.2. Lexemas como `1a`, `1c`, `9z`

Por maximal munch, `1a` seria `INT_CONST(1)` seguido de `ID(a)`, ou seja, erro
sintático. Os testes do enunciado tratam esses casos como erro léxico, e foi
essa a leitura adotada: o lexema é consumido inteiro e reportado como
"identificador não pode começar com dígito". Isso também evita erros em cascata.

---

## 8. Testes

`testes/` contém os programas do enunciado, sem alterações:

| Arquivo | Exercita |
|---|---|
| `teste1.txt` | Comentário `{*` fechado com `}` em vez de `*}`; `1a` como identificador; `is` no lugar do separador de tipo |
| `teste2.txt` | `1c` iniciando com dígito; `:=` no lugar de `=`; float `34.` incompleto |
| `teste3.txt` | Literal `"Par);` não fechado; programa sem `end.` |
| `teste4.txt` | Comentário `{*` nunca fechado; `pontuação` com caracteres não-ASCII; literal quebrado em duas linhas |
| `teste5.txt` | Aspas duplicadas em `"Maior valor: ""`; constante char `'n)` não fechada |
| `teste6_sucesso.txt` | Programa válido: declarações, `while`, operadores lógicos e relacionais, literais, comentário multilinha |
| `teste6_erro.txt` | Erros deliberados: `@` na linha 4 e literal não fechado na linha 7 |

```bash
./compilador testes/teste6_sucesso.txt
```

Erros sintáticos e semânticos desses arquivos (falta de `end.`, variável não
declarada, `if` sem `then`) não são detectados nesta etapa; ficam para as
Etapas 2 e 3.

---

## 9. Estrutura do projeto

```
.
├── token.h           tokens e struct Token
├── token.c           nomes dos tokens e formatação da saída
├── symbol_table.h    interface da Tabela de Símbolos
├── symbol_table.c    tabela hash com encadeamento separado
├── lexer.h           interface do analisador léxico
├── lexer.c           AFD (17 estados) e tratamento de erros
├── main.c            linha de comando e apresentação
├── Makefile
├── README.md
└── testes/           programas de teste do enunciado
```
