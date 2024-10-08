// /* If we want to use other functions, we have to put the relevant
//  * header includes here. */
%{
  int yylex(void);
  void yyerror(const char *s);
  
  #include <stdio.h>
  #include <string.h>
  #include <stdlib.h>
  #include "./inc/Helper.h"

  extern void proc_instruction(int name, ...);
  extern void push_back_list(char* arg);
  extern void clear_list();

  extern int check_end();

  extern void set_output_file(char* name);
%}

/* These declare our output file names. */
%output "parser.c"
%defines "parser.h"

/* This union defines the possible return types of both lexer and
 * parser rules. We'll refer to these later on by the field name */
%union {
  char* str;
  char* operand;
}

%token <str> TOKEN_GLOBAL
%token <str> TOKEN_EXTERN
%token <str> TOKEN_SECTION
%token <str> TOKEN_WORD
%token <str> TOKEN_SKIP
%token <str> TOKEN_ASCII
%token <str> TOKEN_EQU
%token <str> TOKEN_END

// instructions 
%token <str> TOKEN_HALT
%token <str> TOKEN_INT
%token <str> TOKEN_IRET
%token <str> TOKEN_CALL
%token <str> TOKEN_RET
%token <str> TOKEN_JMP
%token <str> TOKEN_BEQ
%token <str> TOKEN_BNE
%token <str> TOKEN_BGT
%token <str> TOKEN_PUSH
%token <str> TOKEN_POP
%token <str> TOKEN_XCHG
%token <str> TOKEN_ADD
%token <str> TOKEN_SUB
%token <str> TOKEN_MUL
%token <str> TOKEN_DIV
%token <str> TOKEN_NOT
%token <str> TOKEN_AND
%token <str> TOKEN_OR
%token <str> TOKEN_XOR
%token <str> TOKEN_SHL
%token <str> TOKEN_SHR
%token <str> TOKEN_LD
%token <str> TOKEN_ST
%token <str> TOKEN_CSRRD
%token <str> TOKEN_CSRWR

// gprX
%token <str> TOKEN_GPRX

// csrX
%token <str> TOKEN_CSRX

// chars
%token <str> TOKEN_PLUS
%token <str> TOKEN_COMMA
%token <str> TOKEN_DOLLAR
%token <str> TOKEN_LEFT_BRACKET
%token <str> TOKEN_RIGHT_BRACKET

// primitive types
%token <str> TOKEN_LITERAL
%token <str> TOKEN_SYMBOL
%token <str> TOKEN_STRING
%token <str> TOKEN_LABEL

// dynamics
%type <operand> operand

%start prog

%%

prog 
    : 
    | prog directive
    | prog instr
    | prog label
    ;

directive
  : TOKEN_GLOBAL  lista_simbola              { proc_instruction(GLOBAL, NULL); }
  | TOKEN_EXTERN  lista_simbola              { proc_instruction(EXTERN, NULL); }
  | TOKEN_SECTION TOKEN_SYMBOL               { proc_instruction(SECTION, $2, NULL); }
  | TOKEN_WORD    lista_simbola_ili_literala { proc_instruction(WORD, NULL); }
  | TOKEN_SKIP    TOKEN_LITERAL              { proc_instruction(SKIP, $2, NULL); }
  | TOKEN_ASCII   TOKEN_STRING               { proc_instruction(ASCII, $2, NULL); }
  | TOKEN_END                                { proc_instruction(END, NULL); }
  ;

instr
  : TOKEN_HALT                                                        { proc_instruction(HALT, NULL); }
  | TOKEN_INT                                                         { proc_instruction(INT, NULL); }
  | TOKEN_IRET                                                        { proc_instruction(IRET, NULL); }
  | TOKEN_CALL  operand                                               { proc_instruction(CALL, $2, NULL); }
  | TOKEN_RET                                                         { proc_instruction(RET, NULL); }
  | TOKEN_JMP   operand                                               { proc_instruction(JMP, $2, NULL); }
  | TOKEN_BEQ   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX TOKEN_COMMA operand { proc_instruction(BEQ, $2, $4, $6, NULL); }
  | TOKEN_BNE   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX TOKEN_COMMA operand { proc_instruction(BNE, $2, $4, $6, NULL); }
  | TOKEN_BGT   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX TOKEN_COMMA operand { proc_instruction(BGT, $2, $4, $6, NULL); }
  | TOKEN_PUSH  TOKEN_GPRX                                            { proc_instruction(PUSH, $2, NULL); }
  | TOKEN_POP   TOKEN_GPRX                                            { proc_instruction(POP, $2, NULL); }
  | TOKEN_XCHG  TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(XCHG, $2, $4, NULL); }
  | TOKEN_ADD   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(ADD, $2, $4, NULL); }
  | TOKEN_SUB   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(SUB, $2, $4, NULL); }
  | TOKEN_MUL   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(MUL, $2, $4, NULL); }
  | TOKEN_DIV   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(DIV, $2, $4, NULL); }
  | TOKEN_NOT   TOKEN_GPRX                                            { proc_instruction(NOT, $2, NULL); }
  | TOKEN_AND   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(AND, $2, $4, NULL); }
  | TOKEN_OR    TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(OR, $2, $4, NULL); }
  | TOKEN_XOR   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(XOR, $2, $4, NULL); }
  | TOKEN_SHL   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(SHL, $2, $4, NULL); }
  | TOKEN_SHR   TOKEN_GPRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(SHR, $2, $4, NULL); }
  | TOKEN_LD    operand    TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(LD, $2, $4, NULL); }
  | TOKEN_ST    TOKEN_GPRX TOKEN_COMMA operand                        { proc_instruction(ST, $2, $4, NULL); }
  | TOKEN_CSRRD TOKEN_CSRX TOKEN_COMMA TOKEN_GPRX                     { proc_instruction(CSRRD, $2, $4, NULL); }
  | TOKEN_CSRWR TOKEN_GPRX TOKEN_COMMA TOKEN_CSRX                     { proc_instruction(CSRWR, $2, $4, NULL); }
  ;

label
  : TOKEN_LABEL { proc_instruction(LABEL, $1, NULL); }
  ;

operand
  : TOKEN_DOLLAR TOKEN_LITERAL { $$ = strcat(strcat($1, $2), "1"); }
  | TOKEN_DOLLAR TOKEN_SYMBOL  { $$ = strcat(strcat($1, $2), "2"); }
  | TOKEN_LITERAL  { $$ = strcat($1, "3"); }
  | TOKEN_SYMBOL { $$ = strcat($1, "4"); }
  | TOKEN_GPRX   { $$ = strcat($1, "5"); }
  | TOKEN_LEFT_BRACKET TOKEN_GPRX TOKEN_RIGHT_BRACKET { $$ = strcat(strcat( strcat($1, $2), $3), "6"); }
  | TOKEN_LEFT_BRACKET TOKEN_GPRX TOKEN_PLUS TOKEN_LITERAL TOKEN_RIGHT_BRACKET { 
    $$ = strcat(strcat( strcat($1, $2), strcat( strcat($3, $4), $5 ) ), "7" ); 
    }
  | TOKEN_LEFT_BRACKET TOKEN_GPRX TOKEN_PLUS TOKEN_SYMBOL TOKEN_RIGHT_BRACKET { 
      $$ = strcat(strcat( strcat($1, $2), strcat( strcat($3, $4), $5 ) ), "8" ); 
  }
  ;

lista_simbola
  : lista_simbola TOKEN_COMMA TOKEN_SYMBOL { push_back_list($3); }
  | TOKEN_SYMBOL { clear_list(); push_back_list($1); }
  ;

lista_simbola_ili_literala
  : lista_simbola_ili_literala TOKEN_COMMA TOKEN_SYMBOL { push_back_list($3); }
  | lista_simbola_ili_literala TOKEN_COMMA TOKEN_LITERAL { push_back_list($3); }
  | TOKEN_SYMBOL { clear_list(); push_back_list($1); }
  | TOKEN_LITERAL { clear_list(); push_back_list($1); }
  ;

%%

void yyerror(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

extern FILE* yyin;
int main(int argc, char* argv[]){
  printf("ASSEMBLER STARTING...\n");

  char *outputFile = NULL;
  char *inputFile = NULL;

  // Traverse command line arguments
  for (int i = 1; i < argc; i++) {
      // Check for the -o option and extract the output file
      if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
          outputFile = argv[i + 1];
          i++;  // Skip next argument since it is the output file
      } else {
          // Remaining argument is the input file
          inputFile = argv[i];
      }
  }
  
  yyin = fopen(inputFile , "r");
  if (!yyin) {
      perror("fopen");
      return 1;
  }

  set_output_file(outputFile);

  yyparse();
  
  if(check_end() == 0){
    printf("ERROR: no .end directive at the end of code!\n");
  }
  
  fclose(yyin);
  return 0;
} 