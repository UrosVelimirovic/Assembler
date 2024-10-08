#include "./../inc/Linker.h"


void Linker::startLinking()
{
    if(hexOption == false && relocatableOption == false){
        cout << "Linker: ERROR -> linker must be started with either -hex or -relocatable option." << endl;
        exit(0);
    }
    if(hexOption == true && relocatableOption == true){
        cout << "Linker: ERROR -> linker must be started with either -hex or -relocatable option. NOT BOTH." << endl;
        exit(0);
    }

    // generate needed structures;
    generate_file_structures();
 
    // error checks
    check_multiple_or_lack_of_symbol_definition();
   
    // generate sections to merge
    find_sections_to_be_merged();

    // generate new tables
    generate_new_tables();
  
    // generate machine codes
    generate_machine_code();

    // generate output.
    if(relocatableOption){
        generate_linkable_elf();
    } else{
        resolve_symbols();
        for(SymbolTableRow* sTemp: symbolTableGeneral){
            if(sTemp->type != SCTN){
                continue;
            }
            //printSectionCode(sTemp->name, sectionMachineCodesGeneral[sTemp->name]);
        }
        generate_executable_elf();
    }
    //printSymbolTable(symbolTableGeneral);
}

void Linker::generate_file_structures()
{
    for(string fileName: fileNames){
      
        // open the file for reading.
        ifstream* fileHandle = open_input_file(fileName);

        // read bytes;
        vector<char> bytes = read_bytes_from_file(fileHandle);
      
        // close the file.
        fileHandle->close();
        delete fileHandle;
        
        // extract ElfHeader.
        Elf32_Ehdr elfHeader = extract_elf_header(bytes);

        // extract Section Header.
        vector<Elf32_Shdr> sectionHeaders = extract_section_headers(bytes, elfHeader.e_shnum);

        // extract shstrtab section data
        vector<char> shstrtab = extract_shstrtab(bytes, sectionHeaders[SHSTRTAB_INDEX].sh_offset, sectionHeaders[SHSTRTAB_INDEX].sh_size);
      
        // extract strtab section data
        vector<char> strtab = extract_strtab(bytes, sectionHeaders[STRTAB_INDEX].sh_offset, sectionHeaders[STRTAB_INDEX].sh_size);
     
        // extract symTab section data
        vector<Elf32_Sym> symtab = extract_symtab(bytes, sectionHeaders[SYMBOLTAB_INDEX]);

        // extract sections
        unordered_map<int, vector<char>> sectionMachineCodes; // int is offset in section for names
        unordered_map<int, vector<Elf32_Rela>> sectionRelaTables;

        for(int i = 3; i < sectionHeaders.size(); i += 2){
            Elf32_Shdr tempMachineCodeHeader = sectionHeaders[i];
            Elf32_Shdr tempRelaHeader = sectionHeaders[i + 1];

            sectionMachineCodes[tempMachineCodeHeader.sh_name] = extract_machine_code_data(bytes, tempMachineCodeHeader);
            sectionRelaTables[tempMachineCodeHeader.sh_name] = extract_rela_data(bytes, tempRelaHeader);
        }


        // generate file Structure.
        fileStructures[fileName] = new FileStructure();

        // symbol table
        vector<SymbolTableRow*> symbolTable;
        unordered_map<string, SymbolTableRow*> symbolTableMap;

        symbolTable.push_back(new SymbolTableRow(0, 0, 0, NOTYP, LOC, UND, "", false));
        
        for(int i = 0; i < symtab.size(); i++){
            string tempName = build_string_from_vector(strtab, symtab[i].st_name);
            SymbolTableRow* temp = new SymbolTableRow(
                    i + 1,
                    symtab[i].st_value,
                    symtab[i].st_size,
                    (ELF32_ST_TYPE(symtab[i].st_info) == STT_NOTYPE? NOTYP:SCTN),
                    (ELF32_ST_BIND(symtab[i].st_info) == STB_LOCAL? LOC:GLOB),
                    ( (symtab[i].st_shndx - 3) / 2 ) + 1,
                    tempName,
                    false
            );
            symbolTable.push_back(temp);
            symbolTableMap[tempName] = temp;
        }
        fileStructures[fileName]->symbolTable = symbolTable;
        fileStructures[fileName]->symbolTableMap = symbolTableMap;

        // machineCodes 
        for (const auto& pair : sectionMachineCodes){
            string sectionNameTemp = build_string_from_vector(shstrtab, pair.first);
            vector<char> data = pair.second;
            fileStructures[fileName]->sectionMachineCodes[sectionNameTemp] = data;
        }

        // rela Tables 

        for (const auto& pair : sectionRelaTables){
            string sectionNameTemp = build_string_from_vector(shstrtab, pair.first);
            vector<RelocationTableRow*> relocationTable;
            for(Elf32_Rela tempRela: pair.second){
                relocationTable.push_back(
                    new RelocationTableRow(
                        tempRela.r_offset,
                        DEFAULT_32_TYPE,
                        tempRela.r_info,
                        tempRela.r_addend
                    )
                );
            }
            fileStructures[fileName]->relocationTables[sectionNameTemp] = relocationTable;
        }
        

        // cout << "________FILE____"<< fileName << endl;
        // printSymbolTable(fileStructures[fileName]->symbolTable);
        // for (const auto& pair : fileStructures[fileName]->sectionMachineCodes) {
        //     printSectionCode(pair.first, pair.second);

        // }

        // for(const auto& pair: fileStructures[fileName]->relocationTables){
        //     printRelocationTable(pair.first, pair.second);
        // }
        // cout << "FILEENDENDENDENDENDENDENDENDENDENDENDENDEND" << endl;
    }
}

void Linker::generate_new_tables()
{
    //vector<string> newSectionOrder;
    symbolTableGeneral.push_back(new SymbolTableRow(0, 0, 0, NOTYP, LOC, UND, "", false));

    // do custom sections.
    if(are_sections_placable()){
       
        for(pair<string, int> place : places){
            for(string fileName: fileNames){
                // for each file traverse its symbol table, find custom sections and add them in
                // correct order. Later we will edit symbols NOTYP and sections that are not custom.
                FileStructure* fileStructureTemp = fileStructures[fileName];
                vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;
                unordered_map<string, SymbolTableRow*> symbolTableMapTemp = fileStructureTemp->symbolTableMap;
                if(symbolTableMapTemp.find(place.first) == symbolTableMapTemp.end()){
                    continue;
                }
                SymbolTableRow* strTemp = symbolTableMapTemp[place.first];
    
                if(strTemp->type != SCTN){
                    continue;
                }
                
                if(section_has_custom_place(strTemp->name) == false){
                    continue;
                }

                // check if the section has already been done (it happens only in case it's a mergable sctn).
                if(symbolTableMapGeneral.find(strTemp->name) != symbolTableMapGeneral.end()){
                    continue;
                }

                int newNum = symbolTableGeneral.size();
                int newValue;
                int newSize;
                SymbolType newType = SCTN;
                SymbolBind newBind = LOC;
                int newNdx = newNum;
                string newName = strTemp->name;


                unsigned int customPlace = get_section_custom_place(strTemp->name);
        
                // check overlapping
                if(customPlace < sectionLocationCounter){
                    cout << "Linker: ERROR -> overlapping during setting custom place for section " << strTemp->name << endl;
                    exit(0);
                }

                newValue = customPlace;

                if(is_section_to_be_merged(strTemp->name)){
                    newSize = generate_size_of_merged_sections(strTemp->name);
                } else {
                    newSize = strTemp->size;
                }

                sectionLocationCounter = newValue + newSize;

                // insert in symbol table
                SymbolTableRow* sTemp = new SymbolTableRow(
                    newNum, newValue, newSize, newType,
                    newBind, newNdx, newName, false
                );
                symbolTableGeneral.push_back(sTemp);
                symbolTableMapGeneral[strTemp->name] = sTemp;
            }
        }
    }

    // do other sections
    for(string fileName: fileNames){
     
        FileStructure* fileStructureTemp = fileStructures[fileName];
        vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;

        for(SymbolTableRow* strTemp: symbolTableTemp){
            if(strTemp->type != SCTN){
                continue;
            }

            if(are_sections_placable() && section_has_custom_place(strTemp->name) == true){
                continue;
            }
            // check if the section has already been done (it happens only in case it's a mergable sctn).
            if(symbolTableMapGeneral.find(strTemp->name) != symbolTableMapGeneral.end()){
                continue;
            }

            int newNum = symbolTableGeneral.size();
            int newValue;
            int newSize;
            SymbolType newType = SCTN;
            SymbolBind newBind = LOC;
            int newNdx = newNum;
            string newName = strTemp->name;

            newValue = sectionLocationCounter;

            if(is_section_to_be_merged(strTemp->name)){
                newSize = generate_size_of_merged_sections(strTemp->name);
            } else {
                newSize = strTemp->size;
            }

            sectionLocationCounter = newValue + newSize;

            // insert in symbol table
            SymbolTableRow* sTemp = new SymbolTableRow(
                newNum, newValue, newSize, newType,
                newBind, newNdx, newName, false
            );
            symbolTableGeneral.push_back(sTemp);
            symbolTableMapGeneral[strTemp->name] = sTemp;
        }
    }
  
    // do other symbols (not sections)
    for(string fileName: fileNames){
      
        FileStructure* fileStructureTemp = fileStructures[fileName];
        vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;

        for(SymbolTableRow* strTemp: symbolTableTemp){
             
            // skip sections
            if(strTemp->type != NOTYP){
                continue;
            }

            // skip default symbol
            if(strTemp->num == 0){
                continue;
            }

            // skip external symbol
            if(strTemp->ndx == 0){
                continue;
            }

            if(strTemp->bind == LOC){
                continue;
            }

            string sectionName = find_section_name_based_on_ndx(symbolTableTemp, strTemp->ndx);
    
            int newNum = symbolTableGeneral.size();
            int newValue;
            int newSize = 0;
            SymbolType newType = NOTYP;
            SymbolBind newBind = strTemp->bind;
            int newNdx = symbolTableMapGeneral[sectionName]->ndx;
            string newName = strTemp->name;
   
            if(is_section_to_be_merged(sectionName)){
                newValue = find_offset_for_symbol_in_a_merged_section(fileName, strTemp->name);
            } else{
                newValue = strTemp->value;
            }

            // insert in symbol table
            SymbolTableRow* sTemp = new SymbolTableRow(
                newNum, newValue, newSize, newType,
                newBind, newNdx, newName, false
            );
            symbolTableGeneral.push_back(sTemp);
            symbolTableMapGeneral[strTemp->name] = sTemp;
        }
    }

    // relaTables
    for(string fileName: fileNames){
      
        FileStructure* fileStructureTemp = fileStructures[fileName];
        unordered_map<string, vector<RelocationTableRow*>> relocationTablesTemp = fileStructureTemp->relocationTables;

        for(const auto& pair: relocationTablesTemp){
            string sectionName = pair.first;
            vector<RelocationTableRow*> relocationTable = pair.second;

            for(RelocationTableRow* rtrTemp: relocationTable){
                int newOffset;
                RelocationType newType = DEFAULT_32_TYPE;
                vector<SymbolTableRow*> oldSymbolTableOfThisFile = fileStructures[fileName]->symbolTable;
                int newSymbol = symbolTableMapGeneral[oldSymbolTableOfThisFile[rtrTemp->symbol]->name]->num;
                int newAddend = -1;

                if(is_section_to_be_merged(symbolTableGeneral[newSymbol]->name)){
                    newAddend = find_new_offset_for_old_offset_in_a_merged_section(fileName, symbolTableGeneral[newSymbol]->name, rtrTemp->addend);
                }else{
                    newAddend = rtrTemp->addend;
                }   
                    

                if(is_section_to_be_merged(sectionName)){
                    newOffset = find_new_offset_for_old_offset_in_a_merged_section(fileName, sectionName, rtrTemp->offset);
                }else{
                    newOffset = rtrTemp->offset;
                }

                relocationTablesGeneral[sectionName].push_back(
                    new RelocationTableRow(
                        newOffset,
                        newType,
                        newSymbol,
                        newAddend
                    )
                );
            }
        }
    }
    
    // machine codes

    //print all
    // printSymbolTable(symbolTableGeneral);

    // for(const auto& pair: relocationTablesGeneral){
    //     printRelocationTable(pair.first, pair.second);
    // }
}

void Linker::check_multiple_or_lack_of_symbol_definition()
{
    set<string> checkSet;
    set<string> externalSymbols;

    for(string fileName: fileNames){
        FileStructure* fileStructureTemp = fileStructures[fileName];
        vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;

        for(SymbolTableRow* strTemp: symbolTableTemp){
            string tempName = strTemp->name;

            // skip sections
            if(strTemp->type == SCTN){
                continue;
            }

            // skip default symbol
            if(strTemp->num == 0){
                continue;
            }

            // skip local symbols
            if(strTemp->bind == LOC){
                continue;
            }

            if(strTemp->ndx == 0){
                externalSymbols.insert(strTemp->name);
            }
            else{
                if(checkSet.find(tempName) != checkSet.end()){
                    cout << "Linker: ERROR -> Multiple definitions of symbol " << tempName  << endl;
                    exit(0);
                }
                checkSet.insert(tempName);
            }
        }
    }
 
    for (const auto& symbol : externalSymbols) {
        if(checkSet.find(symbol) == checkSet.end()){
            cout << "Linker: ERROR -> Symbol " << symbol << " was declared extern but was never defined anywhere." << endl;
            exit(0);
        }
    }
}

void Linker::find_sections_to_be_merged()
{
    set<string> sections;

    for(string fileName: fileNames){
        FileStructure* fileStructureTemp = fileStructures[fileName];
        vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;

        for(SymbolTableRow* strTemp: symbolTableTemp){
            string tempName = strTemp->name;
            if(strTemp->type != SCTN){
                continue;
            }
            if(sections.find(tempName) == sectionsToBeMerged.end()){
                sections.insert(tempName);
            } else {
                sectionsToBeMerged.insert(tempName);
            }
        }
    }
}

bool Linker::is_section_to_be_merged(string name)
{
    if(sectionsToBeMerged.find(name) != sectionsToBeMerged.end()){
        return true;
    }

    return false;
}

bool Linker::section_has_custom_place(string name)
{
    for(pair<string, int> tempPlaces: places){
        string tempName = tempPlaces.first;
        if(name == tempName){
            return true;
        }
    }

    return false;
}

int Linker::get_section_custom_place(string name)
{
    for(pair<string, int> place: places){
        if(place.first == name){
            return place.second;
        }
    }
    return -1;
}

bool Linker::are_sections_placable()
{
    return hexOption;
}

int Linker::generate_size_of_merged_sections(string name)
{
    int size = 0;

    for(string fileName: fileNames){
        FileStructure* fileStructureTemp = fileStructures[fileName];
        vector<SymbolTableRow*> symbolTableTemp = fileStructureTemp->symbolTable;

        for(SymbolTableRow* strTemp: symbolTableTemp){
            if(strTemp->name == name){
                size += strTemp->size;
            }
        }
    }

    return size;
}

string Linker::find_section_name_based_on_ndx(vector<SymbolTableRow *> table, int ndx)
{
    for(SymbolTableRow* temp: table){
        if(temp->type == SCTN && temp->ndx == ndx){
            return temp->name;
        }
    }
    
    cout << "Linker: ERROR -> Could not find section for ndx " << ndx << endl;
    exit(0);
    return "";
}

int Linker::find_offset_for_symbol_in_a_merged_section(string fileName, string symbolName)
{
    string sectionName = find_section_name_based_on_ndx(
        fileStructures[fileName]->symbolTable,
        fileStructures[fileName]->symbolTableMap[symbolName]->ndx
    );
    
    int offset = 0;
    
    for(string fileNameTemp: fileNames){
        FileStructure* fileStructureTemp = fileStructures[fileNameTemp];
        unordered_map<string, SymbolTableRow*> symbolTableMapTemp = fileStructureTemp->symbolTableMap;

        if(fileNameTemp == fileName){
            offset += symbolTableMapTemp[symbolName]->value;
            break;
        } else if(symbolTableMapTemp.find(sectionName) != symbolTableMapTemp.end()){
            offset += symbolTableMapTemp[sectionName]->size;
        } else {
            continue;
        }   
    }

    return offset;
}

int Linker::find_new_offset_for_old_offset_in_a_merged_section(string fileName, string sectionName, int offset)
{
    int newOffset = 0;

    for(string fileNameTemp: fileNames){
        FileStructure* fileStructureTemp = fileStructures[fileNameTemp];
        unordered_map<string, SymbolTableRow*> symbolTableMapTemp = fileStructureTemp->symbolTableMap;

        if(fileNameTemp == fileName){
            newOffset += offset;
            break;
        } else if(symbolTableMapTemp.find(sectionName) != symbolTableMapTemp.end()){
            newOffset += symbolTableMapTemp[sectionName]->size;
        } else {
            continue;
        }   
    }

    return newOffset;
}

void Linker::generate_machine_code()
{
    for(SymbolTableRow* strTemp: symbolTableGeneral){
        if(strTemp->type != SCTN){
            continue;
        }

        string sctnName = strTemp->name;

        vector<unsigned char> machineCode;

        for(string fileName: fileNames){
            FileStructure* fileStructTemp = fileStructures[fileName];
            if(fileStructTemp->sectionMachineCodes.find(sctnName) == fileStructTemp->sectionMachineCodes.end()){
                continue;
            }
            vector<char> tempCode = fileStructTemp->sectionMachineCodes[sctnName];
            machineCode.insert(machineCode.end(), tempCode.begin(), tempCode.end());
            // for(char c: tempCode){
            //     cout << c << endl;
            // }
        }

        sectionMachineCodesGeneral[sctnName] = machineCode;
    }
}

void Linker::resolve_symbols()
{
    // first resolve values of global symbols(not sections)
    for(SymbolTableRow* sTemp: symbolTableGeneral){
        if(sTemp->num == 0){
            continue;
        }
        if(sTemp->type == SCTN){
            continue;
        }

        string hisSectionName = find_section_name_based_on_ndx(symbolTableGeneral, sTemp->ndx);
        uint32_t hisSectionStartAddress = symbolTableMapGeneral[hisSectionName]->value;
        sTemp->value += hisSectionStartAddress;
    }

    for(SymbolTableRow* sTemp: symbolTableGeneral){
        if(sTemp->type != SCTN){
            continue;
        }

        string sectionNameTemp = sTemp->name;

        vector<RelocationTableRow*> relocationTableTemp = relocationTablesGeneral[sectionNameTemp];

        for(RelocationTableRow* rtrTemp: relocationTableTemp){

            int addValue = symbolTableGeneral[rtrTemp->symbol]->value + rtrTemp->addend;

            int oldValue =  build_int_from_chars(
                sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset],
                sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 1],
                sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 2],
                sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 3]
            );

            int finalValue = addValue + oldValue;

            unsigned char byte1 = finalValue;
            unsigned char byte2 = finalValue >> 8;
            unsigned char byte3 = finalValue >> 16;
            unsigned char byte4 = finalValue >> 24; 

            sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset] = byte1;
            sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 1] = byte2;
            sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 2] = byte3;
            sectionMachineCodesGeneral[sectionNameTemp][rtrTemp->offset + 3] = byte4;
        }
    }
}

int Linker::hex_char_to_int(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';  // '0' -> 0, '1' -> 1, ..., '9' -> 9
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;  // 'A' -> 10, 'B' -> 11, ..., 'F' -> 15
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;  // 'a' -> 10, 'b' -> 11, ..., 'f' -> 15
    }
    return -1;  // Return -1 if it's not a valid hex character
}

ifstream *Linker::open_input_file(string fileName)
{
    std::ifstream* inFile= new ifstream(fileName, std::ios::binary);
    // Check if the file was successfully opened
    if (!inFile) {
        cout << "Linker: ERROR -> File " << fileName << " does not exist or cannot be opened!" << endl;
        exit(0);
    }

    return inFile;
}

vector<char> Linker::read_bytes_from_file(ifstream* stream)
{
      // Ensure the file is open and valid
    if (!stream->is_open()) {
        std::cerr << "Error: File not open.\n";
        exit(1);
    }

    // Get the file size
    stream->seekg(0, std::ios::end);
    std::streampos fileSize = stream->tellg();
    stream->seekg(0, std::ios::beg);

    std::vector<char> bytes;
    bytes.reserve(fileSize / 2);  // Reserve space for the bytes (assuming 2 hex chars per byte)

    char hex1, hex2;

    // Read the file content in a for loop
    for (std::streampos pos = 0; pos < fileSize; pos += 2) {
        *stream >> hex1 >> hex2;
        // Convert both hex characters to their integer values
        int value1 = hex_char_to_int(hex1);
        int value2 = hex_char_to_int(hex2);

        // Check if the characters are valid hex digits
        if (value1 == -1 || value2 == -1) {
            std::cerr << "Error: Invalid hex character found: " << hex1 << hex2 << "\n";
            exit(1);
        }

        // Combine the two hex values into a single byte (char)
        unsigned char byte = (value1 << 4) | value2;
        bytes.push_back(byte); 
    }

    return bytes;
}

FILE *Linker::open_input_file_FILE(string fileName)
{
    FILE* file = fopen(fileName.c_str(), "r");
    if (!file) {
        std::cerr << "Failed to open file." << std::endl;
        exit(0);
    }
    return file;
}

vector<char> Linker::read_bytes_from_file_FILE(FILE *file)
{
    // Get the file size
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // Read the file into a buffer
    char* elfFile = new char[fileSize];
    fread(elfFile, 1, fileSize, file);
    fclose(file);
    vector<char> data;
    for(int i = 0; i < fileSize; i += 2){
        char hex1 = elfFile[i];
        char hex2 = elfFile[i + 1];
        int value1 = hex_char_to_int(hex1);
        int value2 = hex_char_to_int(hex2);
        data.push_back( (char)((value1 << 4) | value2) );
    }
    return data;
}

Elf32_Ehdr Linker::extract_elf_header(vector<char> bytes)
{
    Elf32_Ehdr elfHeader;

    int cursor = 0;

    // e_ident
    for(int i = 0; i < EI_NIDENT; i ++){
        elfHeader.e_ident[i] = bytes[i];
        cursor++;
    }

    elfHeader.e_type = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_machine = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_version = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_entry = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_phoff = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;
    
    elfHeader.e_shoff = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;
    
    elfHeader.e_flags = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_ehsize = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_phentsize = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_phnum = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_shentsize = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    elfHeader.e_shnum = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;
    
    elfHeader.e_shstrndx = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
    cursor += 4;

    return elfHeader;
}

int Linker::build_int_from_chars(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4)
{
    int value = 0;
    value |= (int)byte1;
    value |= ( ( (int)byte2 ) << 8);
    value |= ( ( (int)byte3 ) << 16);
    value |= ( ( (int)byte4 ) << 24);
    return value;
}

vector<Elf32_Shdr> Linker::extract_section_headers(vector<char> bytes, int numberOfEntries)
{
    vector<Elf32_Shdr> sectionHeaders;

    int cursor = ELF_HEADER_SIZE;

    for(int i = 0; i < numberOfEntries; i ++){
        Elf32_Shdr sectionHeader;

        sectionHeader.sh_name = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_type = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_flags = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_addr = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_offset = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_size = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_link = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_info = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_addralign = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeader.sh_entsize = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        sectionHeaders.push_back(sectionHeader);
    }
    
    return sectionHeaders;
}

vector<char> Linker::extract_shstrtab(vector<char> bytes, int offset, int size)
{
    vector<char> data;

    for(int i = offset; i < offset + size; i ++){
        
        data.push_back(bytes[i]);
    }

    return data;
}

vector<char> Linker::extract_strtab(vector<char> bytes, int offset, int size)
{
    vector<char> data;

    for(int i = offset; i < offset + size; i ++){
        data.push_back(bytes[i]);
    }

    return data;
}

vector<Elf32_Sym> Linker::extract_symtab(vector<char> bytes, Elf32_Shdr info)
{
    int offset = info.sh_offset;
    int numOfEntries = info.sh_size / info.sh_entsize;
   
    vector<Elf32_Sym> symbolTable;

    int cursor = offset;
    for(int i = 0; i < numOfEntries; i ++){
        Elf32_Sym symbolTableRow;
        symbolTableRow.st_name = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        symbolTableRow.st_info = bytes[cursor];
        cursor ++;

        symbolTableRow.st_other = bytes[cursor];
        cursor++;

        symbolTableRow.st_shndx = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        symbolTableRow.st_value = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        symbolTableRow.st_size = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;

        symbolTable.push_back(symbolTableRow);
    }

    return symbolTable;
}

vector<char> Linker::extract_machine_code_data(vector<char> bytes, Elf32_Shdr info)
{
    int offset = info.sh_offset;
    int size = info.sh_size;

    vector<char> data;

    for(int i = offset; i < offset + size; i ++){
        data.push_back(bytes[i]);
    }
    return data;
}

vector<Elf32_Rela> Linker::extract_rela_data(vector<char> bytes, Elf32_Shdr info)
{
    int offset = info.sh_offset;
    int numOfEntries = info.sh_size / info.sh_entsize;

    int cursor = offset;
    vector<Elf32_Rela> table;
    for(int i = 0; i < numOfEntries; i ++){
        Elf32_Rela tempRow;
        tempRow.r_offset = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;
        tempRow.r_info = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;
        tempRow.r_addend = build_int_from_chars(bytes[cursor], bytes[cursor + 1], bytes[cursor + 2], bytes[cursor + 3]);
        cursor += 4;
        table.push_back(tempRow);
    }

    return table;
}

string Linker::build_string_from_vector(vector<char> bytes, int offset)
{
    string temp = "";

    for(int i = offset; (int)bytes[i] != 0; i++){
        
        temp += bytes[i];
    }

    return temp;
}

void Linker::printSymbolTable(vector<SymbolTableRow *> symbolTable)
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

void Linker::printSectionCode(string name, vector<unsigned char> data)
{  
     std::cout << endl << "SECTION TABLE:" << endl << endl;
     // Print table header
    std::cout << left << setw(15) << "Name"
        << left << setw(10) << "Base"
        << left << setw(10) << "Length"
        << "Data" << endl;

    std::cout << string(50, '-') << endl;  // Print a line for separation

    // Print each section in the table

    std::cout << left << setw(15) << name
        << left << setw(10) << dec << 0
        << left << setw(10) << dec << data.size();

    // Print data in hexadecimal format
    const size_t dataSize = data.size();
    for (size_t i = 0; i < dataSize; ++i) {
        if (i > 0 && i % 8 == 0) {
            std::cout << endl << setw(35) << setfill(' ') << "";  // Align data to the right under the Data header
        }
        
        if(static_cast<int>(static_cast<unsigned char>(data[i])) < 16){
            std::cout << "0";
        }
        std::cout << uppercase << hex << static_cast<int>(static_cast<unsigned char>(data[i])) << " ";
    }
    std::cout << endl;

    std::cout << "___________________________________________________" << endl << endl;
}

void Linker::printRelocationTable(string name, vector<RelocationTableRow *> relocationTable)
{
    const int width_offset = 10;
    const int width_type = 20;
    const int width_symbol = 10;
    const int width_addend = 10;


    cout << "Section Name: " << name << endl;
    cout << "-------------------------------------------------------------" << endl;
    cout << left 
            << setw(width_offset) << "Offset" 
            << setw(width_type) << "Type" 
            << setw(width_symbol) << "Symbol" 
            << setw(width_addend) << "Addend" << endl;
    cout << "-------------------------------------------------------------" << endl;

    for (const auto& row : relocationTable) {
        cout << dec<< left 
                << setw(width_offset) << row->offset 
                << setw(width_type) << to_string(row->type) 
                << setw(width_symbol) << row->symbol 
                << setw(width_addend) << row->addend << endl;
    }

    cout << "-------------------------------------------------------------" << endl;
    
}

void Linker::write_to_file_4bytes_little_endian(ofstream &ostream, Elf32_Word value)
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

void Linker::generate_linkable_elf()
{
    int elfCursor = 0;

    int numberOfCodeSections = 0;
    for(SymbolTableRow* s: symbolTableGeneral){
        if(s->type == SCTN){
            numberOfCodeSections ++;
        }
    }

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
    elfHeader.e_shnum = 3 + 2*numberOfCodeSections; 
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

    for(SymbolTableRow* s: symbolTableGeneral){
        if(s->num == 0){
            continue;
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
            newSectionSym.st_shndx = 0; // can never happen.
        }
        else if(s->ndx == UND){
            newSectionSym.st_shndx = -1;
        }
        else{
            // * 2 to compensate for rela, and + 3 to compensate for first 3 sections which are info sections;
            newSectionSym.st_shndx = (s->ndx - 1) * 2 + 3;  
        }

        // value
        newSectionSym.st_value = (s->type == SCTN? 0:s->value);

        // size
        newSectionSym.st_size = s->size;

        // push
        symtabData.push_back(newSectionSym);
    
        // if its a section we got to have additional stuff.
        if(s->type == SCTN){
            // make code section
            vector<Elf32_Byte> machineCode = sectionMachineCodesGeneral[s->name];

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
            newSectionHeader.sh_size = symbolTableMapGeneral[s->name]->size;
            newSectionHeader.sh_link = 0;
            newSectionHeader.sh_info = 0;
            newSectionHeader.sh_addralign = 0;
            newSectionHeader.sh_entsize = 0;

            codeSections[s->name].first = machineCode;
            codeSections[s->name].second = newSectionHeader;


            // rela
            vector<Elf32_Rela> newRelaTable;
            vector<RelocationTableRow*> oldRelaTable = relocationTablesGeneral[s->name];

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
    symtabHeader.sh_size = (symbolTableGeneral.size() - 1) * SYMTAB_ENTRY_SIZE;
    
    symtabHeader.sh_link = 1;

    //index of last loc symbol
    int tempSymIndex = -1;
    for(SymbolTableRow* str2: symbolTableGeneral){
        if(str2->bind == LOC){
            tempSymIndex = str2->num;
        }
    }
    symtabHeader.sh_info = tempSymIndex + 1;
    symtabHeader.sh_addralign = 0;
    symtabHeader.sh_entsize = SYMTAB_ENTRY_SIZE;

    // other sections
    int offsetCounter = symtabHeader.sh_offset + symtabHeader.sh_size;

    for (SymbolTableRow* strTemp2: symbolTableGeneral) {
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

    const char* fileName = outputFile.c_str();
    
    ofstream outFile(fileName, std::ios::out | std::ios::trunc);

    if (!outFile) {
        std::cout << "Linker: ERROR -> Could not generate an elf relocatable file." << endl;
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
    for (SymbolTableRow* strTemp3: symbolTableGeneral) {
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

    for(SymbolTableRow* tempstr: symbolTableGeneral){
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
}

void Linker::generate_executable_elf()
{
    map<uint32_t, unsigned char> memory;

    for(SymbolTableRow* strTemp: symbolTableGeneral){
        if(strTemp->type != SCTN){
            continue;
        }

        for(int i = 0; i < strTemp->size; i ++){
            memory[strTemp->value + i] = sectionMachineCodesGeneral[strTemp->name][i];
        }
    }


    // generate file

    const char* fileName = outputFile.c_str();
    
    ofstream outFile(fileName, std::ios::out | std::ios::trunc);

    if (!outFile) {
        std::cout << "Linker: ERROR -> Could not generate an elf executable file." << endl;
        exit(0);
    }

    for(const auto& pair: memory){
        outFile << dec << pair.first << ": ";
        outFile << hex << setw(2) << setfill('0') << static_cast<uint32_t>(pair.second) << '\n';
    }
    outFile.close();
}
