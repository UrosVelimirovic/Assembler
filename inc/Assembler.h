#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <climits>
#include <fstream>

#include "elfBase.h"
using namespace std;

#define WORD_SIZE 4

class Assembler {
private:

    // location counter
    int locationCounter;

    // output file;
    string outputFileName;
   
    /**
     * Symbol table
     */
    enum SymbolType
    {
        NOTYP, // no type
        SCTN, // section
        WFI // waiting for initialization. It is used when symbol has been encountered, but hasn't been defined as a label or equ or whatever.
    };

    enum SymbolBind{
        LOC, // local
        GLOB // global
    };

    // undefined symbol index -> NDX; 
    static const int UND = -1;
    static const int EXT = -2;
  
    struct SymbolTableRowStruct
    {
        uint32_t num;
        uint32_t value;
        uint32_t size;
        SymbolType type;
        SymbolBind bind;
        int ndx;
        string name;
        bool equ;

        SymbolTableRowStruct(int num, int value, int size, SymbolType type, SymbolBind bind, int ndx, string name, bool equ):
            num(num), value(value), size(size), type(type), bind(bind), ndx(ndx), name(name), equ(equ){}
    } ;

    typedef SymbolTableRowStruct SymbolTableRow;

    vector<SymbolTableRow*> symbolTable;
    unordered_map<string, SymbolTableRow*> symbolTableMap;

    /**
     * flink
     */
    enum Operation{
        PLUS, MINUS
    };
    struct FLinkActionStruct{
        uint32_t address;
        Operation operation;
        bool st8Relocation;
  

        FLinkActionStruct(uint32_t addressParam, Operation operationParam, bool st8Relocation): 
            address(addressParam), operation(operationParam), st8Relocation(st8Relocation) {}
    };
    typedef FLinkActionStruct FLinkAction; 
    struct FLinkRowStruct{
        string symbolIdentifier;
        int symbolValue;
        vector<FLinkAction> fLinkAction;

        FLinkRowStruct(string symbolIdentifierParam, int symbolValueParam, int addressParam, Operation operationParam, bool st8Relocation):
            symbolIdentifier(symbolIdentifierParam), symbolValue(symbolValueParam)
        {
            fLinkAction.push_back(FLinkActionStruct(addressParam, operationParam, st8Relocation));
        }
    };
    typedef FLinkRowStruct FLinkRow;

    /**
     * Sections
     */
    struct SectionDefinitionStruct
    {
        string name;
        uint32_t base;
        uint32_t length;
        vector<FLinkRow*> fLinkTable;
        unordered_map<string, FLinkRow*> fLinkTableMap;
        vector<uint8_t> data; // machine code

        ~SectionDefinitionStruct(){
            // delete flink table
            for(FLinkRow* f: fLinkTable){
                delete f;
            }
        }
    };
    typedef SectionDefinitionStruct SectionDefinition;

    unordered_map<string, SectionDefinition*> sectionTable;

    string currentSection;

    unsigned int sectionCounter;


    /**
     * relocation table
     */
    enum RelocationType{
        DEFAULT_32_TYPE
    };
    
    struct RelocationTableRowStruct{
        int offset;
        RelocationType type;
        int symbol;
        int addend;

        RelocationTableRowStruct(int offset, RelocationType type, int symbol, int addend):
            offset(offset), type(type), symbol(symbol), addend(addend)
        {}
    };
    typedef RelocationTableRowStruct RelocationTableRow;

    unordered_map<int, vector<RelocationTableRow*>> relocationTables;
    
    /**
     * elf
     */
    vector<Elf32_Byte> elfData;
    int elfCursor;
public:
    Assembler(){
    
        // init location counter
        this->locationCounter = 0;

        // init current section;
        this->currentSection = "0";

        // init section counter
        this->sectionCounter = 0;

        // init symbol table
        SymbolTableRow* s = new SymbolTableRow(0, 0, 0, NOTYP, LOC, UND, "", false);
        this->symbolTable.push_back(s);

    }

    // Directives.
    void global(vector<string> params);
    void externI(vector<string> params);
    void section(string sectionName);
    void word(vector<string> params); // not implemented
    void skip(string param);
    void ascii(string param);
    void equ(); // not implemented
    void end();

    // Instructions.
    void halt();
    void intI();
    void iret();
    void call(string param);
    void ret();
    void jmp(string param);
    void beq(vector<string> params);
    void bne(vector<string> params);
    void bgt(vector<string> params);
    void push(string param);
    void pop(string param);
    void xchg(vector<string> params);
    void add(vector<string> params);
    void sub(vector<string> params);
    void mul(vector<string> params);
    void div(vector<string> params);
    void notI(string param);
    void andI(vector<string> params);
    void orI(vector<string> params);
    void xorI(vector<string> params);
    void shl(vector<string> params);
    void shr(vector<string> params);
    void ld(vector<string> params);
    void st(vector<string> params);
    void csrrd(vector<string> params);
    void csrwr(vector<string> params);

    // label
    void label(string param);
    
    void noOp(){ locationCounter += 4; }

    void set_output_file(char* name);

    ~Assembler(){

        // delete section table.
        for(const auto& pair: sectionTable){
            delete pair.second;
        }

        // delete symbol table.
        for(SymbolTableRow* s: symbolTable){
            delete s;
        }

        // delete relocation table.
        for (const auto& pair : relocationTables) {
            int section = pair.first;
            vector<RelocationTableRow*> row = pair.second;
            for(RelocationTableRow* r: row){
                delete r;
            }
        }

    }
private:
    void generate_relocation_tables();
    void generate_elf_file();
    void check_symbols_at_the_end();
    void edit_section_sizes_in_table_at_the_end();
    
    void printSymbolTable();
    void printSectionTable();
    void printFLinkTable();
    void printRelocationTables();
    string operationToString(Operation op);
    int literal_to_int(string param);
    void insert_word_into_machine_code(int value);
    int general_register_string_to_index(string param);
    int system_register_string_to_index(string param);
    void push_to_flink(string param, int symbolValue, int address, Operation operation, bool st8Relocation);
    void write_to_file_4bytes_little_endian(ofstream& ostream, Elf32_Word value);
    void print_elf(string fileName);
};