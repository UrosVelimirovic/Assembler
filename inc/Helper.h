
#include <stdarg.h>


// extern "C" void proc_instruction(char* name, ...);
// extern "C" void push_back_list(char* arg);
// extern "C" void clear_list();

enum Instruction{
    GLOBAL,
    EXTERN,
    SECTION,
    WORD,
    SKIP,
    ASCII,
    EQU,
    END,
    HALT,
    INT,
    IRET,
    CALL,
    RET,
    JMP,
    BEQ,
    BNE,
    BGT,
    PUSH,
    POP,
    XCHG,
    ADD,
    SUB,
    MUL,
    DIV,
    NOT,
    AND,
    OR,
    XOR,
    SHL,
    SHR,
    LD,
    ST,
    CSRRD,
    CSRWR,
    LABEL
};
