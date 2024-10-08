#include "./../inc/Assembler.h"

void Assembler::global(vector<string> params)
{

    for(string param: params){

        if(symbolTableMap.find(param) == symbolTableMap.end()){
            SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, GLOB, UND, param, false);

            symbolTable.push_back(s);
            symbolTableMap[s->name] = s;
            
        } else {
            if(symbolTableMap[param]->ndx == EXT){
                std::cout << "Assembler: ERROR -> not allowed calling global after extern for the same symbol!" << endl;
                exit(0);
            }

            symbolTableMap[param]->bind = GLOB;
        }
    }
}

void Assembler::externI(vector<string> params)
{
    for(string param: params){
        
        // symbol doesn't exist in the table
        if(symbolTableMap.find(param) == symbolTableMap.end()){
            SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, NOTYP, GLOB, EXT, param, false);

            symbolTable.push_back(s);
            symbolTableMap[s->name] = s;
        } else {   // symbol already exists. Just update it.

            // symbol already initialized with label
            if(symbolTableMap[param]->type != WFI){
                std::cout << "Assembler: ERROR -> Calling extern on already defined symbol " << symbolTableMap[param]->name << endl;
                exit(0);
            }
           
            if(symbolTableMap[param]->bind == GLOB){
                std::cout << "Assembler: ERROR -> not allowed calling extern after global for the same symbol!" << endl;
                exit(0);
            }

            if(symbolTableMap[param]->equ == true){
                std::cout << "Assembler: ERROR -> not allowed calling extern for symbols that have been initialized with equ!" << endl;
                exit(0);
            }
            symbolTableMap[param]->bind = GLOB;
            symbolTableMap[param]->type = NOTYP;
            symbolTableMap[param]->ndx = EXT;
        }

    }
}

void Assembler::section(string sectionName)
{
    // error, symbol with the said name already exists in the map and is initialized.
    if(symbolTableMap.find(sectionName) != symbolTableMap.end() && symbolTableMap[sectionName]->type != WFI){
        std::cout << "Assembler: ERROR, symbol with the name " << sectionName << " already exists in the symbol table" << endl;
        exit(0);
    }        

    sectionCounter++;

    if (currentSection != "0"){
        // set length of section that just ended.
        sectionTable[currentSection]->length = locationCounter - sectionTable[currentSection]->base;
    }
    // reset location counter -> all sections begin at the address 0.
    locationCounter = 0;

    // set current section.
    currentSection = sectionName;
    sectionTable[currentSection] = new SectionDefinition();
    sectionTable[currentSection]->name = sectionName;
    sectionTable[currentSection]->base = locationCounter;

    // add new section in symbol table
    if(symbolTableMap.find(sectionName) == symbolTableMap.end()){
        SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, SCTN, LOC, sectionCounter, sectionName, false);
        symbolTableMap[s->name] = s;
        symbolTable.push_back(s);
    }
    else{
        
        symbolTableMap[sectionName]->type = SCTN;
    }
}

void Assembler::word(vector<string> params)
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> word attempted outside of a section." << endl;
        exit(0);
    }

    for (string param: params){
        
        // literal
        if (param[0] <= 57 && param[0] >= 48){
            int value = literal_to_int(param);

            // Insert into machine code:
            insert_word_into_machine_code(value);

            locationCounter += WORD_SIZE;

        } else { // symbol

            // symbol is not in the table ( add it )
            if (symbolTableMap.find(param) == symbolTableMap.end()){ 
                // add new symbol in symbol table
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, param, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
            }
            // Insert zeroes into machine code.
            insert_word_into_machine_code(0);

            // add to flink.
            push_to_flink(param, -1, locationCounter, PLUS, false);

            // Increase location.
            locationCounter += WORD_SIZE;
        }
    }
}

void Assembler::skip(string param)
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> skip attempted outside of a section." << endl;
        exit(0);
    }

    // Number of bytes to skip.
    
    int skipBytes = literal_to_int(param);

    // Insert zeroes in machine code.
    sectionTable[currentSection]->data.insert(sectionTable[currentSection]->data.end(), skipBytes, 0);

    // Increase location counter.
    locationCounter += skipBytes;
}

void Assembler::ascii(string param)
{

    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> ascii attempted outside of a section." << endl;
        exit(0);
    }

    // Set data in machine code.
    sectionTable[currentSection]->data.insert(sectionTable[currentSection]->data.end(), param.begin(), param.end());
   
    // Increase location counter.
    locationCounter += param.size();
}

void Assembler::equ()
{
    std::cout << "Assembler: Warning -> equ not implemented yet" << endl;
}


void Assembler::end()
{

    if(currentSection == "0"){
        cout << "Assembler: ERROR -> No section was oppened" << endl;
        exit(0);
    }
        // set length of section that just ended.
    sectionTable[currentSection]->length = locationCounter - sectionTable[currentSection]->base;
    check_symbols_at_the_end();
    edit_section_sizes_in_table_at_the_end();

    generate_relocation_tables();
    generate_elf_file();

    // printSymbolTable();
   // printSectionTable();

    // printRelocationTables();
}

void Assembler::halt()
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> halt attempted outside of a section." << endl;
        exit(0);
    }

    insert_word_into_machine_code(0);

    locationCounter += WORD_SIZE;
}

void Assembler::intI()
{
    insert_word_into_machine_code(0x10000000);
    locationCounter += WORD_SIZE;
}

/**
 * pop PC; 
 * pop status;
 * 
 * Emulator: 
 * 1: Instrukcija ucitavanja podatka sp <= sp + 8
 * 1001 MMMM AAAA BBBB CCCC DDDD DDDD DDDD
 * MMMM = 0b0001
 * 2: Instrukcija ucitavanja podatka status <= mem[sp - 4]
 * 1001 MMMM AAAA BBBB CCCC DDDD DDDD DDDD
 * MMMM = 0b0110
 * 3: Instrukcija ucitavanja podatka pc <= mem[sp - 8]
 * 1001 MMMM AAAA BBBB CCCC DDDD DDDD DDDD
 * MMMM = 0b0010
 */
void Assembler::iret()
{
    insert_word_into_machine_code(0x91EE0008);
    insert_word_into_machine_code(0x960E0FFC);
    insert_word_into_machine_code(0x92FE0FF8);
    locationCounter += 3 * WORD_SIZE;
}

void Assembler::call(string param)
{
    
    // Error Check
    if(currentSection == "0"){
        std::cout << "Assembler: ERROR -> calling call outside of section!" << endl;
        exit(0);
    }

    char operandType = param[param.size() - 1];

    // Remove operand type character from the string;
    param.erase(param.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    size_t plusPosition = -1;

    switch(operandType){
        case '3': // literal
            // call [pc + 4]
            // JMP pc + 4
            // 32 bit literal

     
            // call [pc + 4] -> push pc; pc<=mem32[gpr[A]+gpr[B]+D];
            insCode = 0x21F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);
    
            // literal
            
            lit = literal_to_int(param);
             // Insert into machine code;
            insert_word_into_machine_code(lit);
        
            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;

            break;
        case '4': // symbol
            // we translate this to
            // call [pc + 4]
            // JMP pc + 4
            // 32 bit symbol value

            // call [pc + 4] -> push pc; pc<=mem32[gpr[A]+gpr[B]+D];
            insCode = 0x21F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

             // add to symbol table if needed:
            if(symbolTableMap.find(param) == symbolTableMap.end()){
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, param, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
                // add to flink.
                push_to_flink(param, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                // Insert zeroes into machine code;
                insert_word_into_machine_code(0);
            }
            else{
                if(symbolTableMap[param]->type != WFI && symbolTableMap[param]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[param]->value);
                }
                else{
                    // add to flink.
                    push_to_flink(param, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                    // Insert zeroes into machine code;
                    insert_word_into_machine_code(0);
                }
            }

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;
            break;
       
        default:
            std::cout << "Assembler: ERROR -> Unknown operand type for jump instructions: " << operandType << endl;
            exit(0);
    };
}

// pop pc
void Assembler::ret()
{
    // gpr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    int insCode = 0x93FE0004;

    // Insert into machine code.
    insert_word_into_machine_code(insCode);
    locationCounter += WORD_SIZE;
}

// pc <= operand;
void Assembler::jmp(string param)
{
     // Error Check
    if(currentSection == "0"){
        std::cout << "Assembler: ERROR -> calling jmp outside of section!" << endl;
        exit(0);
    }

    char operandType = param[param.size() - 1];

    // Remove operand type character from the string;
    param.erase(param.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    size_t plusPosition = -1;

    
    switch(operandType){
        case '3': // literal
            // jmp [pc]
            // 32 bit literal

     
            // jmp [pc] -> push pc; pc<=mem32[gpr[A]+gpr[B]+D];
            insCode = 0x38F00000;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);
    
            // literal
            
            lit = literal_to_int(param);
             // Insert into machine code;
            insert_word_into_machine_code(lit);

            // Increase location counter.
            locationCounter += 2 * WORD_SIZE;

            break;
        case '4': // symbol
            // we translate this to
            // JMP [pc]
            // 32 bit symbol value

            // jmp [pc] -> push pc; pc<=mem32[gpr[A]+gpr[B]+D];
            insCode = 0x38F00000;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);
          

            // add to symbol table if needed:
            if(symbolTableMap.find(param) == symbolTableMap.end()){
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, param, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
                // add to flink.
                push_to_flink(param, -1, locationCounter + WORD_SIZE, PLUS, false);
                // Insert zeroes into machine code;
                insert_word_into_machine_code(0);
            }
            else{
                if(symbolTableMap[param]->type != WFI && symbolTableMap[param]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[param]->value);
                }
                else{
                    // add to flink.
                    push_to_flink(param, -1, locationCounter + WORD_SIZE, PLUS, false);
                    // Insert zeroes into machine code;
                    insert_word_into_machine_code(0);
                }
            }

            // Increase location counter.
            locationCounter += 2 * WORD_SIZE;
            break;
       
        default:
            std::cout << "Assembler: ERROR -> Unknown operand type for jump instructions: " << operandType << endl;
            exit(0);
    };
}

void Assembler::beq(vector<string> params)
{
    // Error Check
    if(currentSection == "0"){
        std::cout << "Assembler: ERROR -> calling beq outside of section!" << endl;
        exit(0);
    }

    int gpr1 = general_register_string_to_index(params[0]);

    int gpr2 = general_register_string_to_index(params[1]);

    string operand = params[2];
    char operandType = operand[operand.size() - 1];

    // Remove operand type character from the string;
    operand.erase(operand.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    size_t plusPosition = -1;
    
    switch(operandType){
        case '3': // literal
            // beq [pc + 4]
            // jmp 4
            // 32 bit literal

     
            // beq [pc] -> if (gpr[B] == gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x39F << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // literal
            
            lit = literal_to_int(operand);
             // Insert into machine code;
            insert_word_into_machine_code(lit);

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;

            break;
        case '4': // symbol
            // we translate this to
            // beq [pc + 4]
            // jmp 4
            // 32 bit symbol value

            // beq [pc] -> if (gpr[B] == gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x39F << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // add to symbol table if needed:
            if(symbolTableMap.find(operand) == symbolTableMap.end()){
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
                // add to flink.
                push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                // Insert zeroes into machine code;
                insert_word_into_machine_code(0);
            }
            else{
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    // add to flink.
                    push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                    // Insert zeroes into machine code;
                    insert_word_into_machine_code(0);
                }
            }

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;
            break;
       
        default:
            std::cout << "Assembler: ERROR -> Unknown operand type for jump instructions: " << operandType << endl;
            exit(0);
    };
}

void Assembler::bne(vector<string> params)
{
   
    // Error Check
    if(currentSection == "0"){
        std::cout << "Assembler: ERROR -> calling beq outside of section!" << endl;
        exit(0);
    }

    int gpr1 = general_register_string_to_index(params[0]);
    
    int gpr2 = general_register_string_to_index(params[1]);

    string operand = params[2];
    char operandType = operand[operand.size() - 1];

    // Remove operand type character from the string;
    operand.erase(operand.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    size_t plusPosition = -1;
    
    switch(operandType){
        case '3': // literal
            // bne [pc + 4]
            // jmp 4
            // 32 bit literal

     
            // bne [pc] -> if (gpr[B] != gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x3AF << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);
            
            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // literal

            lit = literal_to_int(operand);
             // Insert into machine code;
            insert_word_into_machine_code(lit);

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;

            break;
        case '4': // symbol
            // we translate this to
            // bne [pc + 4]
            // jmp 4
            // 32 bit symbol value

            // bne [pc] -> if (gpr[B] != gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x3AF << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // add to symbol table if needed:
            if(symbolTableMap.find(operand) == symbolTableMap.end()){
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
                // add to flink.
                push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                // Insert zeroes into machine code;
                insert_word_into_machine_code(0);
            }
            else{
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    // add to flink.
                    push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                    // Insert zeroes into machine code;
                    insert_word_into_machine_code(0);
                }
            }

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;
            break;
       
        default:
            std::cout << "Assembler: ERROR -> Unknown operand type for jump instructions: " << operandType << endl;
            exit(0);
    };
}

void Assembler::bgt(vector<string> params)
{
    // Error Check
    if(currentSection == "0"){
        std::cout << "Assembler: ERROR -> calling beq outside of section!" << endl;
        exit(0);
    }

    int gpr1 = general_register_string_to_index(params[0]);
    
    int gpr2 = general_register_string_to_index(params[1]);

    string operand = params[2];
    char operandType = operand[operand.size() - 1];

    // Remove operand type character from the string;
    operand.erase(operand.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    size_t plusPosition = -1;
    
    switch(operandType){
        case '3': // literal
            // bgt [pc + 4]
            // jmp 4
            // 32 bit literal

     
            // bgt [pc] -> if (gpr[B] signed> gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x3BF << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);
    
            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // literal
            
            lit = literal_to_int(operand);
             // Insert into machine code;
            insert_word_into_machine_code(lit);

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;

            break;
        case '4': // symbol
            // we translate this to
            // bgt [pc + 4]
            // jmp 4
            // 32 bit symbol value

            // bgt [pc] -> if (gpr[B] signed> gpr[C]) pc<=mem32[gpr[A]+D];
            insCode = (0x3BF << 20) | (gpr1 << 16) | (gpr2 << 12) | 0x004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // JMP pc + 4 -> pc<=gpr[A]+D;
            insCode = 0x30F00004;
            // Insert into machine code;
            insert_word_into_machine_code(insCode);

            // add to symbol table if needed:
            if(symbolTableMap.find(operand) == symbolTableMap.end()){
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);

                symbolTable.push_back(s);
                symbolTableMap[s->name] = s;
                // add to flink.
                push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                // Insert zeroes into machine code;
                insert_word_into_machine_code(0);
            }
            else{
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    // add to flink.
                    push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                    // Insert zeroes into machine code;
                    insert_word_into_machine_code(0);
                }
            }

            // Increase location counter.
            locationCounter += 3 * WORD_SIZE;
            break;
       
        default:
            std::cout << "Assembler: ERROR -> Unknown operand type for jump instructions: " << operandType << endl;
            exit(0);
    };
}

void Assembler::push(string param)
{
    // gpr[A]<=gpr[A]+D; mem32[gpr[A]]<=gpr[C];
    // A = sp, D = -4, C = gpr

    int reg = general_register_string_to_index(param);
    int insCode = (0x81E0 << 16 ) | (reg << 12 ) | 0xFFC;

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::pop(string param)
{
    // gpr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    // A = reg, B = sp, D = 4

    int reg = general_register_string_to_index(param);
    int insCode = (0x93 << 24) | (reg << 20) | (0xE << 16) | 0x004;

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::xchg(vector<string> params)
{
    // temp<=gpr[B]; gpr[B]<=gpr[C]; gpr[C]<=temp;

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x400 << 20) | (gpr1 << 16) | (gpr2 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::add(vector<string> params)
{
    // : gpr[A]<=gpr[B] + gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x50 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::sub(vector<string> params)
{
    // : gpr[A]<=gpr[B] - gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x51 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::mul(vector<string> params)
{
    // : gpr[A]<=gpr[B] * gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x52 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::div(vector<string> params)
{
    // : gpr[A]<=gpr[B] / gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x53 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::notI(string param)
{
    // gpr[A]<=~gpr[B];
    int gpr = general_register_string_to_index(param);

    int insCode = (0x60 << 24) | (gpr << 20) | (gpr << 16);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::andI(vector<string> params)
{
    // : gpr[A]<=gpr[B] & gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x61 << 24) | (gpr2 << 20) | (gpr1 << 16) | (gpr2 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::orI(vector<string> params)
{
    // : gpr[A]<=gpr[B] & gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (0x62 << 24) | (gpr2 << 20) | (gpr1 << 16) | (gpr2 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::xorI(vector<string> params)
{
    // : gpr[A]<=gpr[B] ^ gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (63 << 24) | (gpr2 << 20) | (gpr1 << 16) | (gpr2 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::shl(vector<string> params)
{
    // : gpr[A]<=gpr[B] << gpr[C];

    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (70 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::shr(vector<string> params)
{
    // : gpr[A]<=gpr[B] >> gpr[C];
    int gpr1 = general_register_string_to_index(params[0]);
    int gpr2 = general_register_string_to_index(params[1]);

    int insCode = (71 << 24) | (gpr2 << 20) | (gpr2 << 16) | (gpr1 << 12);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::ld(vector<string> params)
{
  
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> ld attempted outside of section!" << endl;  
        exit(0);
    }
    int gpr = general_register_string_to_index(params[1]);
    string operand = params[0];
    char operandType = operand[operand.size() - 1];
    operand.erase(operand.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    int reg = -1;
    size_t plusPosition = -1;
    string sym = "";

    switch(operandType){
        case '1': // $literal
            // gpr <= mem[pc + 4]
            // jmp pc + 4
            // literal

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (0xF0004);

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            // remove $.
            operand.erase(0,1);

            // literal
               

            lit = literal_to_int(operand);
            
            // Insert into machine code.
            insert_word_into_machine_code(lit);

            locationCounter += 3 * WORD_SIZE;
            break;
        case '2': // $symbol
            // gpr <= mem[pc + 4]
            // jmp pc + 4
            // symbol value

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (0xF0004);

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            // remove $
            operand.erase(0,1);

            // symbol
            if(symbolTableMap.find(operand) != symbolTableMap.end()){
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    insert_word_into_machine_code(0);
                    push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                }
            }
            else{
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);
                symbolTableMap[s->name] = s;
                symbolTable.push_back(s);

                insert_word_into_machine_code(0);

                push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
            }
            locationCounter += 3 * WORD_SIZE;
            break;
        case '3': // literal
            // ld %r1, [PC+8]
            // ld %r1, [%r1]
            // jmp pc+4
            // literal

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (0xF0008);

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (gpr << 16);
            
            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;
            insert_word_into_machine_code(insCode);

            // literal
            lit = literal_to_int(operand);

            // Insert into machine code.
            insert_word_into_machine_code(lit);

            locationCounter += 4 * WORD_SIZE;
            break;
        case '4': // symbol
            // ld %r1, [PC+8]
            // ld %r1, [%r1]
            // jmp pc+4
            // symbol value

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (0xF0008);

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            insCode = (0x92 << 24) | (gpr << 20) | (gpr << 16);
            
            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;
            insert_word_into_machine_code(insCode);

            // symbol
            if(symbolTableMap.find(operand) != symbolTableMap.end()){
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    insert_word_into_machine_code(0);
                    push_to_flink(operand, -1, locationCounter + 3 * WORD_SIZE, PLUS, false);
                }
            }
            else{
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);
                symbolTableMap[s->name] = s;
                symbolTable.push_back(s);

                insert_word_into_machine_code(0);

                push_to_flink(operand, -1, locationCounter + 3 * WORD_SIZE, PLUS, false);
            }

            locationCounter += 4 * WORD_SIZE;
            break;
        case '5': // %reg
            reg = general_register_string_to_index(operand);

            // gpr[A]<=gpr[B]+D
            // A = gpr, B = reg
            insCode = (0x92 << 24) | (gpr << 20) | (reg << 16);
            // Insert into machine code.
            insert_word_into_machine_code(insCode);
            locationCounter += WORD_SIZE;
            break;
        case '6': //[%reg]
            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1);
            reg = general_register_string_to_index(operand);

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            // A = gpr, B = reg, C = 0, D = 0
            insCode = (0x92 << 24) | (gpr << 20) | (reg << 16);
            // Insert into machine code.
            insert_word_into_machine_code(insCode);
            locationCounter += WORD_SIZE;
            break;
        case '7': //  [%reg + literal]

            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1); 

            plusPosition = operand.find('+');

            reg = general_register_string_to_index(operand.substr(0,plusPosition));
            
            lit = literal_to_int(operand.substr(plusPosition + 1));
            
            if(lit > 2047 || lit < -2048){
                std::cout << "Assembler: ERROR -> ld [%reg + literal] literal bigger than 12 bits" << endl;
                exit(0);
            }

            // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
            // A = gpr, B = reg, C = 0, D = literal
          
            insCode = (0x92 << 24) | (gpr << 20) | (reg << 16) | lit;
            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            locationCounter += WORD_SIZE;
            break;
        case '8':
            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1); 

            plusPosition = operand.find('+');

            reg = general_register_string_to_index(operand.substr(0,plusPosition));
            sym = operand.substr(plusPosition + 1);

            if(symbolTableMap.find(sym) != symbolTableMap.end()){
                if(symbolTableMap[sym]->type != WFI && symbolTableMap[sym]->equ == true){
                    if((symbolTableMap[sym]->value > 2047 || symbolTableMap[sym]->value < -2048)){
                        // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
                        // A = gpr, B = reg, C = 0, D = symbolValue
                    
                        insCode = (0x92 << 24) | (gpr << 20) | (reg << 16) | symbolTableMap[sym]->value;

                        // Insert into machine code.
                        insert_word_into_machine_code(insCode);
                    }
                    else{
                        std::cout << "Assembler: ERROR -> ld [%reg + symbol] symbol value bigger than 12 bits" << endl;
                        exit(0);
                    }
                }
                else{
                     insCode = (0x92 << 24) | (gpr << 20) | (reg << 16);
                    // Insert into machine code.
                    insert_word_into_machine_code(insCode);

                    push_to_flink(sym, -1, locationCounter, PLUS, true);
                }
            }
            else{
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, sym, false);
                symbolTableMap[s->name] = s;
                symbolTable.push_back(s);

                insCode = (0x92 << 24) | (gpr << 20) | (reg << 16);
                // Insert into machine code.
                insert_word_into_machine_code(insCode);
                push_to_flink(sym, -1, locationCounter, PLUS, true);
            }

            locationCounter += WORD_SIZE;
            break;
        default:
            std::cout << "Assembler: ERROR -> ld unknown operand type" << endl;
            exit(0);
    };
}

void Assembler::st(vector<string> params)
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> st attempted outside of section!" << endl;  
        exit(0);
    }

    int gpr = general_register_string_to_index(params[0]);
    string operand = params[1];
    char operandType = operand[operand.size() - 1];
    operand.erase(operand.size() - 1, 1);

    int insCode = -1;
    int lit = -1;
    int reg = -1;
    size_t plusPosition = -1;
    string sym = "";

    switch(operandType){
        case '1': // $literal
            std::cout << "Assembler: ERROR -> Illegal addressing st $literal " << endl;
            exit(0);
        case '2': // $symbol
            std::cout << "Assembler: ERROR -> Illegal addressing st $symbol " << endl;
            exit(0);
        case '3': // literal
            // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
            // jmp pc + 4
            // literal
            
            // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
            insCode = (0x82F0 << 16) | (gpr << 12) | 0x004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            // literal
            lit = literal_to_int(operand);

            // Insert into machine code.
            insert_word_into_machine_code(lit);

            locationCounter += 3 * WORD_SIZE;
            break;
        case '4': // symbol
            // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
            // jmp pc + 4
            // symbol value

            // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
            insCode = (0x82F0 << 16) | (gpr << 12) | 0x004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            //pc<=gpr[A]+D;
            insCode = 0x30F00004;

            // Insert into machine code.
            insert_word_into_machine_code(insCode);


            // symbol
            if(symbolTableMap.find(operand) != symbolTableMap.end()){
                if(symbolTableMap[operand]->type != WFI && symbolTableMap[operand]->equ == true){
                    insert_word_into_machine_code(symbolTableMap[operand]->value);
                }
                else{
                    insert_word_into_machine_code(0);
                    push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
                }
            }
            else{
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, operand, false);
                symbolTableMap[s->name] = s;
                symbolTable.push_back(s);

                insert_word_into_machine_code(0);

                push_to_flink(operand, -1, locationCounter + 2 * WORD_SIZE, PLUS, false);
            }
            locationCounter += 3 * WORD_SIZE;
            break;
        case '5': // %reg
            std::cout << "Assembler: ERROR -> Illegal addressing st %reg " << endl;
            exit(0);
        case '6': //[%reg]
            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1);
            reg = general_register_string_to_index(operand);
          
            // mem32[gpr[A]+gpr[B]+D]<=gpr[C];
            // A = reg, B = 0, C = gpr, D = 0
            insCode = (0x80 << 24) | (reg << 20) | (gpr << 12);
            // Insert into machine code.
            insert_word_into_machine_code(insCode);
            locationCounter += WORD_SIZE;
            break;
        case '7': //  [%reg + literal]
            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1); 

            plusPosition = operand.find('+');

            reg = general_register_string_to_index(operand.substr(0,plusPosition));
            lit = literal_to_int(operand.substr(plusPosition + 1));

            if(lit > 2047 || lit < -2048){
                std::cout << "Assembler: ERROR -> ld [%reg + literal] literal bigger than 12 bits" << endl;
                exit(0);
            }

            // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
            // A = reg, B = 0, C = gpr, D = lit
            insCode = (0x82 << 24) | (reg << 20) | (gpr << 12) | lit;
            // Insert into machine code.
            insert_word_into_machine_code(insCode);

            locationCounter += WORD_SIZE;
            break;
        case '8':
            operand.erase(0,1);
            operand.erase(operand.size() - 1, 1); 

            plusPosition = operand.find('+');

            reg = general_register_string_to_index(operand.substr(0,plusPosition));
            sym = operand.substr(plusPosition + 1);

            if(symbolTableMap.find(sym) != symbolTableMap.end()){
                if(symbolTableMap[sym]->type != WFI && symbolTableMap[sym]->equ == true){
                    if(symbolTableMap[sym]->value > 2047 || symbolTableMap[sym]->value < -2048){
                        // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
                        // A = reg, B = 0, C = gpr, D = symbol value
                        insCode = (0x82 << 24) | (reg << 20) | (gpr << 12) | symbolTableMap[sym]->value;
                        // Insert into machine code.
                        insert_word_into_machine_code(insCode);

                    }
                    else{
                        std::cout << "Assembler: ERROR -> st [%reg + symbol] symbol value bigger than 12 bits" << endl;
                        exit(0);
                    }
                }
                else{
                    insCode = (0x82 << 24) | (reg << 20) | (gpr << 12);
                    // Insert into machine code.
                    insert_word_into_machine_code(insCode);

                    push_to_flink(sym, -1, locationCounter, PLUS, true);
                }
            }
            else{
                SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, 0, 0, WFI, LOC, UND, sym, false);
                symbolTableMap[s->name] = s;
                symbolTable.push_back(s);

                insCode = (0x82 << 24) | (reg << 20) | (gpr << 12);
                // Insert into machine code.
                insert_word_into_machine_code(insCode);

                push_to_flink(sym, -1, locationCounter, PLUS, true);
            }

            locationCounter += WORD_SIZE;
            break;
        default:
            std::cout << "Assembler: ERROR -> st unknown operand type" << endl;
            exit(0);
    };
}

void Assembler::csrrd(vector<string> params)
{

    // gpr[A]<=csr[B];
    int csr = system_register_string_to_index(params[0]);
    int gpr = general_register_string_to_index(params[1]);

    int insCode = (0x90 << 24) | (gpr << 20) | (csr << 16);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::csrwr(vector<string> params)
{
    
    // csr[A]<=gpr[B];

    int gpr = general_register_string_to_index(params[0]);
    int csr = system_register_string_to_index(params[1]);

    int insCode = (0x94 << 24) | (csr << 20) | (gpr << 16);

    insert_word_into_machine_code(insCode);

    locationCounter += WORD_SIZE;
}

void Assembler::label(string param)
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> generating label is not allowed outside of section!" << endl;  
        exit(0);
    }

    // erase :
    param.erase(param.size() - 1, 1);

    // add to symbol table if needed:
    if(symbolTableMap.find(param) == symbolTableMap.end()){
        SymbolTableRow* s = new SymbolTableRow(symbolTable.back()->num + 1, locationCounter, 0, NOTYP, LOC, symbolTableMap[currentSection]->ndx, param, false);

        symbolTable.push_back(s);
        symbolTableMap[s->name] = s;
    } else if(symbolTableMap[param]->type == WFI){
        symbolTableMap[param]->value = locationCounter;
        symbolTableMap[param]->ndx = symbolTableMap[currentSection]->ndx;
        symbolTableMap[param]->type = NOTYP;
    } else {
        std::cout <<"Assembler: ERROR -> Re-initialization of symbol " << param << endl;
        exit(0);
    }
}

void Assembler::set_output_file(char *name)
{
    this->outputFileName = string(name);
}

void Assembler::generate_relocation_tables()
{
    for (const auto& pair : sectionTable) {
        // pick section
        string sectionName = pair.first;
        SectionDefinition* sectionDefinition = pair.second;
        vector<FLinkRow*> table = sectionDefinition->fLinkTable;

        // traverse through flink
        for(FLinkRow* row: table){
            string symName = row->symbolIdentifier;

            // traverse through actions
            for(FLinkAction action: row->fLinkAction){

                if(action.st8Relocation == true){
                    if(symbolTableMap[symName]->type == WFI){
                        std::cout << "Assembler: ERROR -> call8Relocation not possible: symbol "
                            << symName << " hasn't been defined completely during assembling." << endl;
                        exit(0);
                    }
                    else{
                        std::cout << "Assembler: ERROR -> impossible(not yet implemented) branch 1 reached during relocation table generation. " << endl;
                        exit(0);
                        // todo: implement this if equ is implemented.
                    }
                } else {
                    if(symbolTableMap[symName]->bind == LOC ){
                        
                        if(symbolTableMap[symName]->ndx == UND){
                            std::cout <<"Assembler: ERROR -> equ not implemented: symbol "
                                << symName << " has bind LOC and ndx UND during relocation table generation." << endl;
                            exit(0);
                        }
                        
                        // find section num
                        int sectionNdx = symbolTableMap[symName]->ndx;
                        int sectionNumTemp = -1;
                        for (const auto& pair2 : sectionTable){
                            string sectionName2 = pair2.first;
                            if(symbolTableMap[sectionName2]->ndx == sectionNdx){
                                sectionNumTemp = symbolTableMap[sectionName2]->num;
                                break;
                            }
                        }

                        relocationTables[symbolTableMap[sectionDefinition->name]->ndx].push_back(
                            new RelocationTableRow(
                                action.address, 
                                DEFAULT_32_TYPE, 
                                sectionNumTemp, 
                                symbolTableMap[symName]->value
                            )
                        );
                    }
                    else if(symbolTableMap[symName]->bind == GLOB){
                        relocationTables[symbolTableMap[sectionDefinition->name]->ndx].push_back(
                            new RelocationTableRow(
                                action.address, 
                                DEFAULT_32_TYPE, 
                                symbolTableMap[symName]->num, 
                                0
                            )
                        );        
                    } else {
                        std::cout << "Assembler: ERROR -> unkown symbol bind " << symbolTableMap[symName]->bind
                            << " for symbol: " << symName << endl;
                        exit(0);
                    }
                }
            }
        }
    }
}

void Assembler::generate_elf_file()
{
    elfCursor = 0;

    Elf32_EhdrStruct elfHeader;
    
    elfHeader.e_ident[0] = 127;
    elfHeader.e_ident[1] = 'E';
    elfHeader.e_ident[2] = 'L';
    elfHeader.e_ident[3] = 'F';
    for(int i = 4; i <= 15; i ++){
        elfHeader.e_ident[i] = 0;
    }
    elfHeader.e_type = ET_REL;
    elfHeader.e_machine = EM_SSEMU_32;
    elfHeader.e_version = 0;
    elfHeader.e_entry = 0;
    elfHeader.e_phoff = 0;
    elfHeader.e_shoff = ELF_HEADER_SIZE;
    elfHeader.e_flags = 0;
    elfHeader.e_ehsize = ELF_HEADER_SIZE;
    elfHeader.e_phentsize = 0;
    elfHeader.e_phnum = 0;
    elfHeader.e_shentsize = SECTION_HEADER_ENTRY_SIZE; 
    elfHeader.e_shnum = 3 + 2*sectionTable.size(); 
    elfHeader.e_shstrndx = SHSTRTAB_INDEX;
    
    // section headers.
    vector<Elf32_Byte> shstrtabData;
    Elf32_Shdr shstrtabHeader;

    int shstrtabLocationCounter = 0;
    // increase 
    for(char c: string("shstrtab")){
        shstrtabData.push_back(c);
    }
    shstrtabData.push_back(0);
    shstrtabLocationCounter += string("shstrtab").size() + 1;

    for(char c: string("strtab")){
        shstrtabData.push_back(c);
    }
    shstrtabData.push_back(0);
    shstrtabLocationCounter += string("strtab").size() + 1;
    
    for(char c: string("symtab")){
        shstrtabData.push_back(c);
    }
    shstrtabData.push_back(0);
    shstrtabLocationCounter += string("symtab").size() + 1;

    // strtab
    vector<Elf32_Byte> strtabData;
    Elf32_Shdr strtabHeader;
    int strtabLocationCounter = 0;

    // symtab
    vector<Elf32_Sym> symtabData;
    Elf32_Shdr symtabHeader;

    unordered_map<string, pair<vector<Elf32_Byte>, Elf32_Shdr>> codeSections;
    unordered_map<string, pair<vector<Elf32_Rela>, Elf32_Shdr>> relaSections;

    for(SymbolTableRow* s: symbolTable){
        if(s->num == 0){
            continue;
        }

        if(s->type == WFI){
            std::cout <<"Assembler: ERROR -> Symbol " << s->name << " not yet defined at the end of assembling." << endl;
            exit(0);
        }

        // put in symtab data
        Elf32_Sym newSectionSym;

        // name 
        for(char c: s->name){
            strtabData.push_back(c);
        }
        strtabData.push_back(0);
        newSectionSym.st_name = strtabLocationCounter;
        strtabLocationCounter += s->name.size() + 1;

        // type and bind
        Elf32_Word bind = s->bind == LOC? STB_LOCAL: STB_GLOBAL;
        Elf32_Word type = s->type == NOTYP? STT_NOTYPE: STT_SECTION;

        newSectionSym.st_info = ELF32_ST_INFO(bind, type);

        // other
        newSectionSym.st_other = 0;

        // shndx
        if(s->ndx == EXT){
            newSectionSym.st_shndx = 0;
        }
        else if(s->ndx == UND){
            newSectionSym.st_shndx = -1;
        }
        else{
            // * 2 to compensate for rela, and + 3 to compensate for first 3 sections which are info sections;
            newSectionSym.st_shndx = (s->ndx - 1) * 2 + 3;  
        }

        // value
        newSectionSym.st_value = s->value;

        // size
        newSectionSym.st_size = s->size;

        // push
        symtabData.push_back(newSectionSym);
    
        // if its a section we got to have additional stuff.
        if(s->type == SCTN){
            // make code section
            vector<Elf32_Byte> machineCode = sectionTable[s->name]->data;

            // make section header.
            Elf32_Shdr newSectionHeader;
            newSectionHeader.sh_name = shstrtabLocationCounter;

            for(char c: s->name){
                shstrtabData.push_back(c);
            }
            shstrtabData.push_back(0);
            shstrtabLocationCounter += s->name.size() + 1;

            
            newSectionHeader.sh_type = SHT_MACHINECODE;
            newSectionHeader.sh_flags = 0;
            newSectionHeader.sh_addr = 0;
            newSectionHeader.sh_offset = -1; // todo
            newSectionHeader.sh_size = sectionTable[s->name]->length;
            newSectionHeader.sh_link = 0;
            newSectionHeader.sh_info = 0;
            newSectionHeader.sh_addralign = 0;
            newSectionHeader.sh_entsize = 0;

            codeSections[s->name].first = machineCode;
            codeSections[s->name].second = newSectionHeader;


            // rela
            vector<Elf32_Rela> newRelaTable;
            vector<RelocationTableRow*> oldRelaTable = relocationTables[s->ndx];

            for(RelocationTableRow* rtr: oldRelaTable){
                Elf32_Rela newRela;
                newRela.r_offset = rtr->offset;
                newRela.r_info = rtr->symbol;
                newRela.r_addend = rtr->addend;
                newRelaTable.push_back(newRela);
            }

            Elf32_Shdr newSectionHeaderRela;

            newSectionHeaderRela.sh_name = shstrtabLocationCounter;
            string newSectionHeaderRelaName = string(".rela.") + s->name;
    
            for(char c: newSectionHeaderRelaName){
                shstrtabData.push_back(c);
            }
            shstrtabData.push_back(0);
            shstrtabLocationCounter += newSectionHeaderRelaName.size() + 1;

            // type
            newSectionHeaderRela.sh_type = SHT_RELA;
            newSectionHeaderRela.sh_flags = 0;
            newSectionHeaderRela.sh_addr = 0;
            newSectionHeaderRela.sh_offset = -1; // todo
            newSectionHeaderRela.sh_size = newRelaTable.size() * RELA_TABLE_ENTRY_SIZE;
            
            newSectionHeaderRela.sh_link = 2;
            newSectionHeaderRela.sh_info = newSectionSym.st_shndx + 1;
            newSectionHeaderRela.sh_addralign = 0;
            newSectionHeaderRela.sh_entsize = RELA_TABLE_ENTRY_SIZE;

            relaSections[s->name].first = newRelaTable;
            relaSections[s->name].second = newSectionHeaderRela;
        }
    }

    // finalize:
    int sectionHeaderTableSize = elfHeader.e_shnum * sizeof(Elf32_Shdr);
    

    // shstrtabHeader
    shstrtabHeader.sh_name = 0;
    shstrtabHeader.sh_type = SHT_STRTAB;
    shstrtabHeader.sh_flags = 0;
    shstrtabHeader.sh_addr = 0;
    shstrtabHeader.sh_offset = ELF_HEADER_SIZE + sectionHeaderTableSize;
    shstrtabHeader.sh_size = shstrtabLocationCounter;
    shstrtabHeader.sh_link = 0;
    shstrtabHeader.sh_info = 0;
    shstrtabHeader.sh_addralign = 0;
    shstrtabHeader.sh_entsize = 0;

    // strtabHeader
    strtabHeader.sh_name = shstrtabHeader.sh_name + string("shstrtab").size() + 1;
    strtabHeader.sh_type = SHT_STRTAB;
    strtabHeader.sh_flags = 0;
    strtabHeader.sh_addr = 0;
    strtabHeader.sh_offset = ELF_HEADER_SIZE + sectionHeaderTableSize + shstrtabLocationCounter;
    strtabHeader.sh_size = strtabLocationCounter;
    strtabHeader.sh_link = 0;
    strtabHeader.sh_info = 0;
    strtabHeader.sh_addralign = 0;
    strtabHeader.sh_entsize = 0;

    // symtabHeader
    symtabHeader.sh_name = strtabHeader.sh_name + string("strtab").size() + 1;
    symtabHeader.sh_type = SHT_SYMTAB;
    symtabHeader.sh_flags = 0;
    symtabHeader.sh_addr = 0;
    symtabHeader.sh_offset = ELF_HEADER_SIZE + sectionHeaderTableSize + shstrtabLocationCounter + strtabLocationCounter;
    symtabHeader.sh_size = (symbolTable.size() - 1) * SYMTAB_ENTRY_SIZE;
    
    symtabHeader.sh_link = 1;

    //index of last loc symbol
    int tempSymIndex = -1;
    for(SymbolTableRow* str2: symbolTable){
        if(str2->bind == LOC){
            tempSymIndex = str2->num;
        }
    }
    symtabHeader.sh_info = tempSymIndex + 1;
    symtabHeader.sh_addralign = 0;
    symtabHeader.sh_entsize = SYMTAB_ENTRY_SIZE;

    // other sections
    int offsetCounter = symtabHeader.sh_offset + symtabHeader.sh_size;

    for (SymbolTableRow* strTemp2: symbolTable) {
        if(strTemp2->type != SCTN){
            continue;
        }
        string sectionName = strTemp2->name;
        
        // machine
        Elf32_Shdr* sectionHeaderTemp = &(codeSections[sectionName].second);
        sectionHeaderTemp->sh_offset = offsetCounter;
        offsetCounter += sectionHeaderTemp->sh_size;
     
        // rela
        Elf32_Shdr* sectionHeaderTempRela = &(relaSections[sectionName].second);
        sectionHeaderTempRela->sh_offset = offsetCounter;
           

        offsetCounter += sectionHeaderTempRela->sh_size;
    }

    // generate file:

    const char* fileName = outputFileName.c_str();
    
    ofstream outFile(fileName, std::ios::out | std::ios::trunc);

    if (!outFile) {
        std::cout << "Assembler: ERROR -> Could not generate an elf file." << endl;
        exit(0);
    }

    // write elf header
    for( int i = 0; i < EI_NIDENT; i ++){
        outFile << hex << setw(2) << setfill('0') << (int)(elfHeader.e_ident[i]);
    }
    
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_type);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_machine);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_version);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_entry);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_phoff);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_shoff);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_flags);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_ehsize);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_phentsize);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_phnum);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_shentsize);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_shnum);
    write_to_file_4bytes_little_endian(outFile, elfHeader.e_shstrndx);
    
    // write shstrtab header
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_name);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_type);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_flags);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_addr);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_offset);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_size);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_link);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_info);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_addralign);
    write_to_file_4bytes_little_endian(outFile, shstrtabHeader.sh_entsize);

    // write strtab header
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_name);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_type);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_flags);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_addr);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_offset);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_size);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_link);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_info);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_addralign);
    write_to_file_4bytes_little_endian(outFile, strtabHeader.sh_entsize);

    // write symtab header
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_name);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_type);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_flags);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_addr);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_offset);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_size);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_link);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_info);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_addralign);
    write_to_file_4bytes_little_endian(outFile, symtabHeader.sh_entsize);

    // write other section headers.
    for (SymbolTableRow* strTemp3: symbolTable) {
        if(strTemp3->type != SCTN){
            continue;
        }
        string sectionName = strTemp3->name;

        // machine
        Elf32_Shdr sectionHeaderTemp = codeSections[sectionName].second;
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_name);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_type);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_flags);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_addr);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_offset);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_size);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_link);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_info);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_addralign);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTemp.sh_entsize);

        // rela
        Elf32_Shdr sectionHeaderTempRela = relaSections[sectionName].second;
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_name);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_type);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_flags);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_addr);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_offset);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_size);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_link);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_info);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_addralign);
        write_to_file_4bytes_little_endian(outFile, sectionHeaderTempRela.sh_entsize);
    }

    // write sections data
  
    for(int i = 0; i < shstrtabData.size(); i ++){
        outFile << hex << setw(2) << setfill('0') << static_cast<int>(shstrtabData[i]);
    }

    for(int i = 0; i < strtabData.size(); i ++){
        outFile << hex << setw(2) << setfill('0') << static_cast<int>(strtabData[i]);
    }

    for(int i = 0; i < symtabData.size(); i ++){
        write_to_file_4bytes_little_endian(outFile, symtabData[i].st_name);
        outFile << hex << setw(2) << setfill('0') << static_cast<int>(symtabData[i].st_info);
        outFile << hex << setw(2) << setfill('0') << static_cast<int>(symtabData[i].st_other);
        write_to_file_4bytes_little_endian(outFile, symtabData[i].st_shndx);
        write_to_file_4bytes_little_endian(outFile, symtabData[i].st_value);
        write_to_file_4bytes_little_endian(outFile, symtabData[i].st_size);
    }

    for(SymbolTableRow* tempstr: symbolTable){
        if(tempstr->type != SCTN){
            continue;
        }

        for(int i = 0; i < codeSections[tempstr->name].first.size(); i ++){
            outFile << hex << setw(2) << setfill('0') << static_cast<int>(codeSections[tempstr->name].first[i]);

        }
      
        for(int i = 0; i < relaSections[tempstr->name].first.size(); i ++){
            
            write_to_file_4bytes_little_endian(outFile, relaSections[tempstr->name].first[i].r_offset);
            write_to_file_4bytes_little_endian(outFile, relaSections[tempstr->name].first[i].r_info);
            write_to_file_4bytes_little_endian(outFile, relaSections[tempstr->name].first[i].r_addend);
        }
        
    }

    outFile.close();

    std::cout << "Assembler: ELF file generated!" << endl;
}

void Assembler::check_symbols_at_the_end()
{
    for(SymbolTableRow* sTemp: symbolTable){
        if(sTemp->type == WFI){
            cout << "Assembler: ERROR -> symbol " << sTemp->name << "left undefined" << endl;
            exit(0);
        }
    }
}

void Assembler::edit_section_sizes_in_table_at_the_end()
{
    for(const auto& pair: sectionTable){
        string sectionName = pair.first;
        int sectionSize = pair.second->length;
        symbolTableMap[sectionName]->size = sectionSize;
    }
}

void Assembler::printSymbolTable()
{
    std::cout << endl << "SYMBOL TABLE:" << endl << endl;
    // Print table header
    std::cout << left << setw(5) << "Num"
        << left << setw(10) << "Value"
        << left << setw(10) << "Size"
        << left << setw(10) << "Type"
        << left << setw(10) << "Bind"
        << left << setw(5) << "Ndx"
        << left << setw(15) << "Name"
        << left << setw(10) << "Equ" << endl;

    std::cout << string(60, '-') << endl;  // Print a line for separation

    // Print each row in the table
    for (const auto& row : symbolTable) {
        std::cout << left << setw(5) << dec << row->num
            << left << setw(10) << dec << row->value
            << left << setw(10) << dec << row->size
            << left << setw(10) << (row->type == NOTYP ? "NOTYP" :
                                    (row->type == WFI? "WFI":"SCTN") )
            << left << setw(10) << (row->bind == LOC ? "LOC" : "GLOB")
            << left << setw(5) << (row->ndx == UND ? "UND" : (row->ndx == EXT? "EXT":to_string(row->ndx)))
            << left << setw(15) << row->name 
            << left << setw(15) << (row->equ == true? "true":"false") << endl;
    }
    std::cout << "___________________________________________________" << endl << endl;
}

void Assembler::printSectionTable()
{
    std::cout << endl << "SECTION TABLE:" << endl << endl;
     // Print table header
    std::cout << left << setw(15) << "Name"
        << left << setw(10) << "Base"
        << left << setw(10) << "Length"
        << "Data" << endl;

    std::cout << string(50, '-') << endl;  // Print a line for separation

    // Print each section in the table
    for (const auto& entry : sectionTable) {
        const SectionDefinition* section = entry.second;

        std::cout << left << setw(15) << section->name
            << left << setw(10) << dec << section->base
            << left << setw(10) << dec << section->length;

        // Print data in hexadecimal format
        const size_t dataSize = section->data.size();
        for (size_t i = 0; i < dataSize; ++i) {
            if (i > 0 && i % 8 == 0) {
                std::cout << endl << setw(35) << setfill(' ') << "";  // Align data to the right under the Data header
            }
            
            if(static_cast<int>(static_cast<unsigned char>(section->data[i])) < 16){
                std::cout << "0";
            }
            std::cout << uppercase << hex << static_cast<int>(static_cast<unsigned char>(section->data[i])) << " ";
        }
        std::cout << endl;
    }
    std::cout << "___________________________________________________" << endl << endl;
}

void Assembler::printFLinkTable()
{
    for(const auto& entry : sectionTable){
        std::cout << "SECTION " << entry.second->name << " ";
        std::cout << endl << "FLINK TABLE:" << endl << endl;
        
        for (const auto& row : entry.second->fLinkTable) {
            std::cout << "Symbol Identifier: " << row->symbolIdentifier << endl;
            std::cout << "Symbol Value: " << dec << row->symbolValue << endl;

            for (const auto& action : row->fLinkAction) {
                std::cout << "  Action Address: " << dec << action.address << ", Operation: " << operationToString(action.operation) << ", ST8: " << (action.st8Relocation == true? "true":"false") << endl;
            }
            std::cout << "-----------------------------------" << endl;
        }
    }
    std::cout << "___________________________________________________" << endl << endl;

}

void Assembler::printRelocationTables()
{
       const int width_offset = 10;
    const int width_type = 20;
    const int width_symbol = 10;
    const int width_addend = 10;

    for (const auto& pair: relocationTables) {
        int sectionId = pair.first;
        vector<RelocationTableRow*> rows = pair.second;
        std::cout << "Section ID: " << sectionId << endl;
        std::cout << "-------------------------------------------------------------" << endl;
        std::cout << left 
             << setw(width_offset) << "Offset" 
             << setw(width_type) << "Type" 
             << setw(width_symbol) << "Symbol" 
             << setw(width_addend) << "Addend" << endl;
        std::cout << "-------------------------------------------------------------" << endl;

        for (const auto& row : rows) {
            std::cout << dec<< left 
                 << setw(width_offset) << row->offset 
                 << setw(width_type) << to_string(row->type) 
                 << setw(width_symbol) << row->symbol 
                 << setw(width_addend) << row->addend << endl;
        }

        std::cout << "-------------------------------------------------------------" << endl;
    }
}

string Assembler::operationToString(Operation op)
{
    switch(op) {
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        default: return "UNKNOWN";
    }
}

int Assembler::literal_to_int(string param)
{

    int number = 0;
    int base = -1;
    if (param.size() > 2 && (param[1] == 'x' || param[1] == 'X') ){
       
       base = 16;   // hexadecimal
    }
    else if (param.size() > 2 && (param[1] == 'b' || param[1] == 'B') ){
        
       base = 2;// binary
    } else {
       base = 10; // decimal
    }
    unsigned long check = std::stoul(param, nullptr, base);
    if(check >> 32 != 0){
        std::cout << "Assembler: ERROR -> literal bigger than 32bits!";
        exit(0);
    }

    number = static_cast<int>(check);
    
   
    return number;
}

void Assembler::insert_word_into_machine_code(int value)
{
    char firstByte = value & 0x000000FF;
    char secondByte = (value >> 8) & 0x000000FF;
    char thirdByte = (value >> 16) & 0x000000FF;
    char fourthByte = (value >> 24) & 0x000000FF;

    sectionTable[currentSection]->data.push_back((char)firstByte);
    sectionTable[currentSection]->data.push_back((char)secondByte);
    sectionTable[currentSection]->data.push_back((char)thirdByte);
    sectionTable[currentSection]->data.push_back((char)fourthByte);

    
}

int Assembler::general_register_string_to_index(string param)
{
    param.erase(0,1);
    if(param == "sp"){
    
        return 0xE;
       
    } else if(param == "pc"){
        return 0xF;
    } else {
        param.erase(0,1);
      
        return stoi(param);
    }

    return -1;
}

int Assembler::system_register_string_to_index(string param)
{
    param.erase(0,1);

    if(param == "status"){
        return 0;
    } else if ( param == "handler"){
        return 1;
    } else if ( param == "cause"){
        return 2;
    } else {
        std::cout << "Assembler: ERROR -> unknown system register " << param << endl;
        exit(0);
    }
}

void Assembler::push_to_flink(string param, int symbolValue, int address, Operation operation, bool st8Relocation)
{
    // Error
    if (currentSection == "0"){
        std::cout << "assembler: ERROR -> pushing to flink attempted outside of section." << endl;  
        exit(0);
    }

    vector<FLinkRow*>& fLinkTable = sectionTable[currentSection]->fLinkTable;
    unordered_map<string, FLinkRow*>& fLinkTableMap = sectionTable[currentSection]->fLinkTableMap;
    if(fLinkTableMap.find(param) == fLinkTableMap.end()){
        FLinkRow* f = new FLinkRow(param, symbolValue, address, operation, st8Relocation);
        fLinkTable.push_back(f);
        fLinkTableMap[param] = f;
    }
    else{
        fLinkTableMap[param]->fLinkAction.push_back(FLinkAction(address, operation, st8Relocation));
    }
}

void Assembler::write_to_file_4bytes_little_endian(ofstream &ostream, Elf32_Word value)
{
    uint8_t firstByte = value & 0x000000FF;
    uint8_t secondByte = (value >> 8) & 0x000000FF;
    uint8_t thirdByte = (value >> 16) & 0x000000FF;
    uint8_t fourthByte = (value >> 24) & 0x000000FF;
    ostream << hex << setw(2) << setfill('0') << static_cast<int>(firstByte);
    ostream << hex << setw(2) << setfill('0') << static_cast<int>(secondByte);
    ostream << hex << setw(2) << setfill('0') << static_cast<int>(thirdByte);
    ostream << hex << setw(2) << setfill('0') << static_cast<int>(fourthByte);
}

void Assembler::print_elf(string fileName)
{

    // Open the binary file
    std::ifstream file(fileName, std::ios::binary);

    // Check if the file was opened successfully
    if (!file) {
        std::cerr << "Assembler: Error -> Could not open the elf file while printing it onto the console!" << std::endl;
        exit(0);
    }

    // Read 8 bytes at a time and print in the desired format
    const std::size_t chunkSize = 8;
    char buffer[chunkSize];

    while (file.read(buffer, chunkSize) || file.gcount() > 0) {
        std::size_t bytesRead = file.gcount();

        // Print 4 bytes in hexadecimal
        for (std::size_t i = 0; i < 4; ++i) {
            if (i < bytesRead) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (static_cast<unsigned int>(static_cast<unsigned char>(buffer[i])));
                std::cout << " ";
            }
            else {
                std::cout << "  ";  // Padding if fewer than 4 bytes are read
            }
        }

        std::cout << "   ";  // Space separator

        // Print next 4 bytes in hexadecimal
        for (std::size_t i = 4; i < chunkSize; ++i) {
            if (i < bytesRead) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (static_cast<unsigned int>(static_cast<unsigned char>(buffer[i])));
                std::cout << " ";
            }
            else {
                std::cout << "  ";  // Padding if fewer than 4 bytes are read
            }
        }

        std::cout << std::endl;
    }

    // Close the file
    file.close();
}
