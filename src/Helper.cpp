
#include "./../inc/Helper.h"
#include "./../inc/Assembler.h"
#include <stdio.h>
#include <iostream>
#include <vector>
using namespace std;


/** 
 * Arguments. 
 */
vector<string> argumentsList;

/**
 * Assembler
 */
Assembler assembler;

/**
 * Global boolean that checks whether or not we encountered .end in code.
 */

bool endEncountered = false;


/**
 * Function for processing instructions. 
 */
extern "C" void proc_instruction(Instruction name, ...){

  va_list args;

  va_start(args, name);

  // traverse through arguments and add them to argument list if they were not added previously;
  if(name != WORD && name != EXTERN && name != GLOBAL){
    argumentsList.clear();
    for( const char* str = va_arg(args, const char*); str != NULL; str = va_arg(args, const char*)){
      if(name == GLOBAL) cout << str;
      argumentsList.push_back(str);
    }
  }

  va_end(args);

  // process instruction.

  switch(name){
    case GLOBAL:
      assembler.global(argumentsList);
      break;
    case EXTERN:
      assembler.externI(argumentsList);
      break;
    case SECTION:
      assembler.section(argumentsList[0]);
      break;
    case WORD:
      assembler.word(argumentsList);
      break;
    case SKIP:
      assembler.skip(argumentsList[0]);
      break;
    case ASCII:
      assembler.ascii(argumentsList[0].substr(1, argumentsList[0].size() - 2)); // strip off of ""
      break;
    case EQU:
      assembler.equ(); 
      break;
    case END:
      assembler.end();
      endEncountered = true;
      cout << "Assembling done!" << endl;
      break;
    case HALT:
      assembler.halt();
      break;
    case INT:
      assembler.intI();
      break;
    case IRET:
      assembler.iret();
      break;
    case CALL:
      assembler.call(argumentsList[0]);
      break;
    case RET:
      assembler.ret();
      break;
    case JMP:
      assembler.jmp(argumentsList[0]);
      break;
    case BEQ:
      assembler.beq(argumentsList);
      break;
    case BNE:
      assembler.bne(argumentsList);
      break;
    case BGT:
      assembler.bgt(argumentsList);
      break;
    case PUSH:
      assembler.push(argumentsList[0]);
      break;
    case POP:
      assembler.pop(argumentsList[0]);
      break;
    case XCHG:
      assembler.xchg(argumentsList);
      break; 
    case ADD:
      assembler.add(argumentsList);
      break; 
    case SUB:
      assembler.sub(argumentsList);
      break; 
    case MUL:
      assembler.mul(argumentsList);
      break; 
    case DIV:
      assembler.div(argumentsList);
      break; 
    case NOT:
      assembler.notI(argumentsList[0]);
      break; 
    case AND:
      assembler.andI(argumentsList);
      break; 
    case OR:
      assembler.orI(argumentsList);
      break; 
    case XOR:
      assembler.xorI(argumentsList);
      break; 
    case SHL:
      assembler.shl(argumentsList);
      break; 
    case SHR:
      assembler.shr(argumentsList);
      break;
    case LD:
      assembler.ld(argumentsList);
      break; 
    case ST:
      assembler.st(argumentsList);
      break;
    case CSRRD:
      assembler.csrrd(argumentsList);
      break;
    case CSRWR:
      assembler.csrwr(argumentsList);
      break;
    case LABEL:
      assembler.label(argumentsList[0]);
      break;
    default:
      cout << "Assembler: ERROR -> unknown instruction with index " << name << endl;
      exit(0);
      break;
  };
}

/**
 * Function for pushing argument to a global list of arguments during parsing.
 */
extern "C" void push_back_list(char* arg){

  argumentsList.push_back(arg);
}

/**
 * When we begin pushing arguments to a global list, we have to clear it with this function.
 */
extern "C" void clear_list(){
  argumentsList.clear();
}

/**
 * Check if we have .end directive at the end of assembly
 */
extern "C" int check_end(){
  return (endEncountered == true)? 1:0;
}

extern "C" void set_output_file(char* name){
  assembler.set_output_file(name);
}