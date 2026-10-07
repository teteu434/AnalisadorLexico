# Compilador — Etapa 1: Analisador Léxico e Tabela de Símbolos

Implementação em **C99 puro**, sem Flex/Lex ou qualquer gerador de analisador,
do analisador léxico da linguagem definida no enunciado da disciplina de
Compiladores (CEFET-MG, Prof.ª Kecia Marques, 2026/2).

O programa recebe um arquivo fonte, varre-o caractere a caractere por meio de um
**Autômato Finito Determinístico (AFD)** e produz:

1. a sequência completa de tokens reconhecidos;
2. o conteúdo da Tabela de Símbolos (palavras reservadas e identificadores
   instalados, com seus índices);
3. a relação dos erros léxicos, cada um com a linha e a coluna exatas.

---

## 1. Como compilar

### Com `make` (GCC / MinGW / Linux)

```bash
make
```

Gera o executável `compilador` (ou `compilador.exe` no Windows). As flags usadas
são `-std=c99 -Wall -Wextra -pedantic -O2`; a compilação é limpa, sem avisos.

Outros alvos:

```bash
make clean                       # remove objetos e executável
make run FILE=testes/teste1.txt  # compila e já analisa um arquivo
```

### Sem `make`, chamando o GCC direto

```bash
gcc -std=c99 -Wall -Wextra -pedantic -O2 -o compilador main.c lexer.c symbol_table.c token.c
```

> **Não tem GCC no Windows?** Instale o [w64devkit](https://github.com/skeeto/w64devkit/releases)
> ou o MinGW-w64 e adicione a pasta `bin` ao `PATH`. Não é preciso MSYS2 nem WSL.

### Com o MSVC (Visual Studio)

```bat
cl /nologo /W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:compilador.exe main.c lexer.c symbol_table.c token.c
```

O `/utf-8` e o `/D_CRT_SECURE_NO_WARNINGS` só silenciam particularidades do
MSVC (ele marca `fopen` e `strncpy` como "inseguras", embora sejam padrão ISO C).
O código-fonte é **ASCII puro**, justamente para compilar igual nos dois toolchains.

---

## 2. Como executar

```
compilador <arquivo-fonte> [opções]
```

Exemplos:

```bash
./compilador testes/teste6_sucesso.txt
./compilador testes/teste6_erro.txt -p
./compilador testes/teste2.txt -i
```

### Opções

| Opção | Efeito |
|---|---|
| *(nenhuma)* | Varre o arquivo inteiro e lista **todos** os erros léxicos encontrados |
| `-p`, `--parar-no-erro` | Encerra a análise no **primeiro** erro léxico |
| `-i`, `--identificadores` | Na Tabela de Símbolos, omite as palavras reservadas |
| `-t`, `--tokens` | Exibe apenas a sequência de tokens |
| `-s`, `--simbolos` | Exibe apenas a Tabela de Símbolos |
| `-h`, `--help` | Mostra a ajuda |

### Códigos de saída

| Código | Significado |
|---|---|
| `0` | Sucesso — nenhum erro léxico |
| `1` | Foram encontrados erros léxicos |
| `2` | Uso incorreto da linha de comando |
| `3` | Falha ao abrir o arquivo ou falta de memória |

### Sobre o tratamento de erro

O enunciado permite encerrar a compilação no primeiro erro ("não é necessário
implementar recuperação de erro"), mas exige que **o relatório** apresente todos
os erros do programa analisado. Os dois comportamentos estão disponíveis:

- **Padrão (listar tudo):** útil para ter a visão geral de quantos problemas
  existem antes de começar a corrigir.
- **`-p` (parar no primeiro):** reproduz o ciclo que o relatório pede —
  mostra o erro, você corrige o fonte, roda de novo e documenta o próximo.

Atenção a um efeito real: **um erro pode mascarar os seguintes**. No Teste 1 do
enunciado, o comentário `{*` que nunca fecha consome o arquivo inteiro, e por
isso um único erro é reportado; depois de corrigido, outros aparecem. Por isso o
ciclo *corrige → roda de novo* continua sendo o jeito correto de montar o
relatório, mesmo com o modo "listar tudo" disponível.

---

## 3. Formato da saída

### Sequência de tokens

Cada token é impresso no formato `<NOME_TOKEN, VALOR_ATRIBUTO>`:

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

A convenção do atributo é:

- **Identificadores e palavras reservadas** → o **índice** da entrada na Tabela
  de Símbolos (`<ID, 15>`, `<PROGRAM, 0>`). É o atributo clássico de um
  identificador, e é o que liga o token à sua entrada na tabela.
- **Demais tokens** → o próprio lexema, entre aspas (`<PLUS, "+">`,
  `<FLOAT_CONST, "10.5">`).

### Tabela de Símbolos

```
INDICE  LEXEMA                    TOKEN           CLASSE          LINHA   USOS
------  ------------------------  --------------  --------------  ------  ----
0       program                   PROGRAM         PALAVRA-RESV    2       1
11      while                     WHILE           PALAVRA-RESV    11      1
15      testeSeis                 ID              IDENTIFICADOR   2       1
16      x                         ID              IDENTIFICADOR   3       6
```

As 15 palavras reservadas ocupam sempre os índices **0 a 14**; os
identificadores do programa começam no índice **15**. A coluna `LINHA` registra
a primeira ocorrência (`-` quando a palavra reservada não aparece no fonte) e
`USOS` conta quantas vezes o símbolo foi reconhecido.

### Erros léxicos

Cada erro traz a mensagem, a posição exata e a própria linha do fonte com um
marcador sob o ponto do problema:

```
[1] Linha 4, coluna 9: simbolo invalido '@'
       4 |   opcao @ : int; {* Erro lexico na linha 4: caracter invalido '@' *}
         |         ^
```

---

## 4. Arquitetura

Quatro módulos, com responsabilidades separadas:

| Arquivo | Propósito |
|---|---|
| `token.h` / `token.c` | Define o `enum TokenType` (41 categorias léxicas + `EOF` + `ERRO_LEXICO`) e a `struct Token`. Concentra a nomenclatura dos tokens e a formatação `<NOME_TOKEN, VALOR_ATRIBUTO>`. |
| `symbol_table.h` / `symbol_table.c` | Tabela de Símbolos: tabela hash com encadeamento separado, alocação dinâmica, pré-carga das palavras reservadas, instalação de identificadores e impressão. |
| `lexer.h` / `lexer.c` | O AFD propriamente dito. Expõe `Token get_next_token(Lexer *)`, além da inicialização e liberação do analisador. |
| `main.c` | Interface de linha de comando: lê os argumentos, conduz o laço de varredura e organiza a apresentação (tokens, erros, tabela, resumo). |

### `struct Token`

```
type          categoria léxica
lexeme        texto reconhecido
line, column  posição de início (1-based)
symbol_index  índice na Tabela de Símbolos, ou -1
symbol        ponteiro para a entrada na tabela, ou NULL
message       diagnóstico, quando type == TOK_ERROR
```

`token.h` apenas **declara** `struct Symbol` (tipo incompleto), evitando
dependência circular com `symbol_table.h`.

### O analisador léxico (AFD)

O arquivo inteiro é carregado em memória, o que torna o *lookahead* e o
*retract* triviais: basta avançar ou recuar o índice de leitura. A função
`get_next_token()` é um único laço de transição de estados que roda até atingir
um estado final, quando devolve o `Token` montado.

São **17 estados**:

| Estado | Papel |
|---|---|
| `S_START` | q0 — descarta brancos e decide a categoria pelo primeiro caractere |
| `S_ID` | lendo identificador ou palavra reservada |
| `S_INT`, `S_FLOAT_DOT`, `S_FLOAT` | constantes numéricas; `S_FLOAT_DOT` é o estado que exige ao menos um dígito após o ponto |
| `S_LITERAL` | dentro de `"..."` |
| `S_CHAR_BODY`, `S_CHAR_CLOSE` | dentro de `'c'` |
| `S_EQUAL`, `S_LESS`, `S_GREATER`, `S_BANG` | operadores com segundo caractere opcional (`=` vs `==`, `<` vs `<=`, …) |
| `S_AMP`, `S_PIPE` | `&&` e `\|\|`; um `&` ou `\|` sozinho é erro |
| `S_BRACE`, `S_COMMENT`, `S_COMMENT_STAR` | comentários `{* ... *}`, inclusive multilinha |

Os estados com segundo caractere opcional fazem o lookahead sem consumir: só
avançam quando o casamento ocorre. É o *retract* clássico, de graça.

A classificação do alfabeto (`is_letter`, `is_digit`, …) é feita com comparações
explícitas, **não** com `<ctype.h>`: `isalpha()` depende de *locale* e pode
aceitar bytes acima de 127, enquanto a gramática define `letter ::= [A-Za-z]`.

### A Tabela de Símbolos

Tabela hash com **encadeamento separado**, 211 buckets (primo) e função **djb2**.
Cada entrada guarda lexema, tipo, índice, linha da primeira ocorrência e número
de usos.

Duas decisões que valem nota:

- **As palavras reservadas são pré-carregadas** em `symtab_init()`, antes de
  qualquer leitura do fonte. Com isso, a distinção entre identificador e palavra
  reservada sai de graça: o lexer lê o lexema, chama `symtab_install()` e copia
  `sym->type`. Não existe nenhuma cadeia de `if` comparando strings.
- Além dos buckets, a tabela mantém uma **lista de ordem de inserção**, para que
  a impressão seja determinística (essencial para o relatório) e para que a
  liberação de memória percorra cada entrada exatamente uma vez.

A busca é **case-sensitive**, como a linguagem exige: `write` é palavra
reservada, enquanto `Write` e `WRITE` entram como identificadores distintos.

---

## 5. Tokens reconhecidos

| Categoria | Tokens |
|---|---|
| Identificador | `ID` — `(letter \| "_") (letter \| digit \| "_")*` |
| Palavras reservadas | `program`, `begin`, `end`, `int`, `float`, `char`, `if`, `then`, `else`, `repeat`, `until`, `while`, `do`, `read`, `write` |
| Constantes | `INT_CONST` (`digit+`), `FLOAT_CONST` (`digit+ "." digit+`), `CHAR_CONST` (`'c'`), `LITERAL` (`"..."`) |
| Relacionais | `==` `!=` `>` `>=` `<` `<=` |
| Aditivos | `+` `-` `\|\|` |
| Multiplicativos | `*` `/` `%` `&&` |
| Unário / atribuição | `!` `=` |
| Delimitadores | `;` `,` `(` `)` `.` `:` |

Comentários `{* ... *}` são descartados pelo analisador (podendo abranger várias
linhas), mas a contagem de linhas é atualizada normalmente dentro deles.

---

## 6. Erros léxicos detectados

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
| Identificador começando com dígito | `1a`, `1c` | `lexema mal formado "1a": identificador nao pode comecar com digito` |
| `&` ou `\|` isolado | `a & b` | `simbolo invalido '&': o operador logico e "&&"` |

Em todos os casos o analisador se recupera (descartando o trecho ofensivo) e
prossegue, de modo que uma única execução possa listar todos os erros.

---

## 7. Decisões de projeto

Duas ambiguidades do enunciado foram resolvidas por decisão explícita, e não por
dedução. **Ambas merecem ser confirmadas com a professora**, porque mudam a
saída dos testes.

### 7.1. O caractere `:`

A gramática do enunciado escreve `decl ::= ident-list "=" type`, e `:` não
aparece em nenhuma produção. Porém **todos** os programas de teste do enunciado
usam `:` como separador de declaração (`base, altura : float;`) e apontam como
errados *outros* trechos da mesma linha (`area, 1a is float;`). Isso indica que
o `:` é a forma pretendida e que a produção `decl` traz uma inconsistência.

- **Padrão adotado:** `:` é reconhecido como o token `COLON`; cabe ao analisador
  sintático (Etapa 2) aceitá-lo em `decl`. Como consequência, `val := 34`
  produz `COLON` seguido de `ASSIGN` — erro **sintático**, não léxico.
- **Para seguir a gramática ao pé da letra:** compile com `-DLEX_ACCEPT_COLON=0`
  e o `:` volta a ser símbolo inválido. Veja o comentário no topo de `lexer.c`.

### 7.2. Lexemas como `1a`, `1c`, `9z`

Por *maximal munch* puro, `1a` seria `INT_CONST(1)` seguido de `ID(a)` — ou
seja, um erro sintático, não léxico. Os testes do enunciado, porém, claramente
tratam esses casos como erro léxico. Adotamos a segunda leitura: o analisador
consome o lexema inteiro e reporta *"identificador não pode começar com
dígito"*, evitando ainda uma cascata de erros derivados.

---

## 8. Arquivos de teste

A pasta `testes/` contém os programas do enunciado, transcritos sem alteração:

| Arquivo | O que exercita |
|---|---|
| `teste1.txt` | Comentário `{*` fechado com `}` em vez de `*}`; `1a` como identificador; `is` no lugar do separador de tipo |
| `teste2.txt` | `1c` iniciando com dígito; `:=` no lugar de `=`; constante float `34.` incompleta |
| `teste3.txt` | Literal `"Par);` não fechado; programa sem `end.` |
| `teste4.txt` | Comentário `{*` nunca fechado; `pontuação` com caracteres não-ASCII; literal quebrado em duas linhas |
| `teste5.txt` | Aspas duplicadas em `"Maior valor: ""`; constante char `'n)` não fechada |
| `teste6_sucesso.txt` | Programa **válido**: declarações, `while`, operadores lógicos e relacionais, literais e comentário multilinha |
| `teste6_erro.txt` | Erros deliberados: `@` inválido (linha 4) e literal não fechado (linha 7) |

Para rodar um deles:

```bash
./compilador testes/teste6_sucesso.txt
```

Lembre-se de que erros sintáticos e semânticos presentes nesses arquivos
(faltar `end.`, variável não declarada, `if` sem `then`) **não** são apontados
nesta etapa — ficam para as Etapas 2 e 3.

---

## 9. Estrutura do projeto

```
.
├── token.h           definição dos tokens e da struct Token
├── token.c           nomes dos tokens e formatação da saída
├── symbol_table.h    interface da Tabela de Símbolos
├── symbol_table.c    tabela hash com encadeamento separado
├── lexer.h           interface do analisador léxico
├── lexer.c           o AFD (17 estados) e o tratamento de erros
├── main.c            linha de comando e apresentação
├── Makefile          compilação
├── README.md         este arquivo
└── testes/           programas de teste do enunciado
```
