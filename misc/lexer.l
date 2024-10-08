%{
#include "parser.h"

%}

%option noyywrap
%option outfile="lexer.c" header-file="lexer.h"


/* Define directive patterns */
GLOBAL   ".global"
EXTERN   ".extern"
SECTION  ".section"
WORD     ".word"
SKIP     ".skip"
ASCII    ".ascii"
EQU      ".equ"
END      ".end"

/* Define instruction patterns */
HALT     "halt"
INT      "int"
IRET     "iret"
CALL     "call"
RET      "ret"
JMP      "jmp"
BEQ      "beq"
BNE      "bne"
BGT      "bgt"
PUSH     "push"
POP      "pop"
XCHG     "xchg"
ADD      "add"
SUB      "sub"
MUL      "mul"
DIV      "div"
NOT      "not"
AND      "and"
OR       "or"
XOR      "xor"
SHL      "shl"
SHR      "shr"
LD       "ld"
ST       "st"
CSRRD    "csrrd"
CSRWR    "csrwr"

/* Define register patterns */
GPRX     "%"("r1"|"r2"|"r3"|"r4"|"r5"|"r6"|"r7"|"r8"|"r9"|"r10"|"r11"|"r12"|"r13"|"r14"|"r15"|"sp"|"pc")
CSRX     "%"("status"|"handler"|"cause")

/* Define character patterns */
PLUS     "+"
COMMA    ","
DOLLAR   "$"
LEFT_BRACKET  "["
RIGHT_BRACKET "]"

/* Define other patterns */
WHITE_SPACE  [ \r\n\t]+
COMMENT      "#"[^\n\r]*

LITERAL      (0[xX][0-9a-fA-F]+|0[bB][01]+|[0-9]+)
LABEL        [A-Za-z_][A-Za-z0-9_]*":"
SYMBOL       [A-Za-z_][A-Za-z0-9_]*
STRING       \"([^"]|\\.)*\" 


%%


{WHITE_SPACE}       { /* Ignore whitespace */ }
{COMMENT}           { /* Ignore comments */ }

{GLOBAL}    { yylval.str = strdup(yytext); return TOKEN_GLOBAL; }
{EXTERN}    { yylval.str = strdup(yytext); return TOKEN_EXTERN; }
{SECTION}   { yylval.str = strdup(yytext); return TOKEN_SECTION; }
{WORD}      { yylval.str = strdup(yytext); return TOKEN_WORD; }
{SKIP}      { yylval.str = strdup(yytext); return TOKEN_SKIP; }
{ASCII}     { yylval.str = strdup(yytext); return TOKEN_ASCII; }
{EQU}       { yylval.str = strdup(yytext); return TOKEN_EQU; }
{END}       { yylval.str = strdup(yytext); return TOKEN_END; }

{HALT}      { yylval.str = strdup(yytext); return TOKEN_HALT; }
{INT}       { yylval.str = strdup(yytext); return TOKEN_INT; }
{IRET}      { yylval.str = strdup(yytext); return TOKEN_IRET; }
{CALL}      { yylval.str = strdup(yytext); return TOKEN_CALL; }
{RET}       { yylval.str = strdup(yytext); return TOKEN_RET; }
{JMP}       { yylval.str = strdup(yytext); return TOKEN_JMP; }
{BEQ}       { yylval.str = strdup(yytext); return TOKEN_BEQ; }
{BNE}       { yylval.str = strdup(yytext); return TOKEN_BNE; }
{BGT}       { yylval.str = strdup(yytext); return TOKEN_BGT; }
{PUSH}      { yylval.str = strdup(yytext); return TOKEN_PUSH; }
{POP}       { yylval.str = strdup(yytext); return TOKEN_POP; }
{XCHG}      { yylval.str = strdup(yytext); return TOKEN_XCHG; }
{ADD}       { yylval.str = strdup(yytext); return TOKEN_ADD; }
{SUB}       { yylval.str = strdup(yytext); return TOKEN_SUB; }
{MUL}       { yylval.str = strdup(yytext); return TOKEN_MUL; }
{DIV}       { yylval.str = strdup(yytext); return TOKEN_DIV; }
{NOT}       { yylval.str = strdup(yytext); return TOKEN_NOT; }
{AND}       { yylval.str = strdup(yytext); return TOKEN_AND; }
{OR}        { yylval.str = strdup(yytext); return TOKEN_OR; }
{XOR}       { yylval.str = strdup(yytext); return TOKEN_XOR; }
{SHL}       { yylval.str = strdup(yytext); return TOKEN_SHL; }
{SHR}       { yylval.str = strdup(yytext); return TOKEN_SHR; }
{LD}        { yylval.str = strdup(yytext); return TOKEN_LD; }
{ST}        { yylval.str = strdup(yytext); return TOKEN_ST; }
{CSRRD}     { yylval.str = strdup(yytext); return TOKEN_CSRRD; }
{CSRWR}     { yylval.str = strdup(yytext); return TOKEN_CSRWR; }



{PLUS}          { yylval.str = strdup(yytext); return TOKEN_PLUS; }
{COMMA}         { yylval.str = strdup(yytext); return TOKEN_COMMA; }
{DOLLAR}        { yylval.str = strdup(yytext); return TOKEN_DOLLAR; }


{LABEL}      { yylval.str = strdup(yytext); return TOKEN_LABEL; }
{SYMBOL}     { yylval.str = strdup(yytext); return TOKEN_SYMBOL; } 
{LITERAL}    { yylval.str = strdup(yytext); return TOKEN_LITERAL; }
{GPRX}      { yylval.str = strdup(yytext); return TOKEN_GPRX; }
{CSRX}      { yylval.str = strdup(yytext); return TOKEN_CSRX; }
{STRING}     { yylval.str = strdup(yytext); return TOKEN_STRING; }
{LEFT_BRACKET}  { yylval.str = strdup(yytext); return TOKEN_LEFT_BRACKET; }
{RIGHT_BRACKET} { yylval.str = strdup(yytext); return TOKEN_RIGHT_BRACKET; }

%%


