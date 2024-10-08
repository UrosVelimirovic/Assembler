#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <iomanip>
#include <string>
#include <set>
#include <algorithm>
#include "elfBase.h"
using namespace std;


class Linker{
private:
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

    struct FileStructureStruct{
        vector<SymbolTableRow*> symbolTable;
        unordered_map<string, SymbolTableRow*> symbolTableMap;

        unordered_map<string, vector<char>> sectionMachineCodes;

        unordered_map<string, vector<RelocationTableRow*>> relocationTables;
    };

    typedef FileStructureStruct FileStructure;

    unordered_map<string, FileStructure*> fileStructures;

    //------------------------------------------------------
    vector<string> fileNames;

    vector<pair<string, uint32_t>> places;

    string outputFile;

    bool hexOption;

    bool relocatableOption;

    //------------------------------------------
    vector<SymbolTableRow*> symbolTableGeneral;

    unordered_map<string, SymbolTableRow*> symbolTableMapGeneral;

    unordered_map<string, vector<unsigned char>> sectionMachineCodesGeneral;

    unordered_map<string, vector<RelocationTableRow*>> relocationTablesGeneral;
    // ---------------------------------------------

    unsigned int sectionLocationCounter;

    set<string> sectionsToBeMerged;
public:
    Linker(vector<string> fileNames, vector<pair<string, uint32_t>> places, string outputFile, bool hexOption, bool relocatableOption){
        this->fileNames = fileNames;
        this->places = places;
        this->outputFile = outputFile;
        this->hexOption = hexOption;
        this->relocatableOption = relocatableOption;
        this->sectionLocationCounter = 0;
        
    
    }

    void startLinking();

private:
    void generate_file_structures();
    void generate_new_tables();
    void check_multiple_or_lack_of_symbol_definition();
    void find_sections_to_be_merged();
    bool is_section_to_be_merged(string name);
    bool section_has_custom_place(string name);
    int get_section_custom_place(string name);
    bool are_sections_placable();
    int generate_size_of_merged_sections(string name);
    string find_section_name_based_on_ndx(vector<SymbolTableRow*> table, int ndx);
    int find_offset_for_symbol_in_a_merged_section(string fileName, string symbolName);
    int find_new_offset_for_old_offset_in_a_merged_section(string fileName, string sectionName, int offset);
    void generate_machine_code();
    void resolve_symbols();

    int hex_char_to_int(char ch);
    ifstream* open_input_file(string fileName);
    vector<char> read_bytes_from_file(ifstream* stream);
    FILE* open_input_file_FILE(string fileName);
    vector<char> read_bytes_from_file_FILE(FILE* file);
    Elf32_Ehdr extract_elf_header(vector<char> bytes);
    int build_int_from_chars(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4);
    vector<Elf32_Shdr> extract_section_headers(vector<char> bytes, int numberOfEntries);
    vector<char> extract_shstrtab(vector<char> bytes, int offset, int size);
    vector<char> extract_strtab(vector<char> bytes, int offset, int size);
    vector<Elf32_Sym> extract_symtab(vector<char> bytes, Elf32_Shdr info);
    vector<char> extract_machine_code_data(vector<char> bytes, Elf32_Shdr info);
    vector<Elf32_Rela> extract_rela_data(vector<char> bytes, Elf32_Shdr info);
    string build_string_from_vector(vector<char> bytes, int offset);

    void printSymbolTable(vector<SymbolTableRow*> symbolTable);
    void printSectionCode(string name, vector<unsigned char> data);
    void printRelocationTable(string name, vector<RelocationTableRow*> relocationTable );
    void write_to_file_4bytes_little_endian(ofstream& ostream, Elf32_Word value);

    void generate_linkable_elf();
    void generate_executable_elf();
};


int literal_to_int_main(string param)
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
        std::cout << "Linker: ERROR -> literal bigger than 32bits!";
        exit(0);
    }

    number = static_cast<int>(check);
    
   
    return number;
}

int main(int argc, char *argv[]){
    cout << "LINKER STARTING..." << endl << endl;

    std::string outputFile;
    bool hexFlag = false, relocatableFlag = false;

    // To store section placement information
    struct SectionPlacement {
        std::string sectionName;
        std::string address;
    };
    std::vector<SectionPlacement> sectionPlacements;

    // To store input file names
    std::vector<std::string> inputFiles;

    // Process the command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-o" && i + 1 < argc) {
            // Handle the -o <naziv_izlazne_datoteke> option
            outputFile = argv[++i];
        } else if (arg.rfind("-place=", 0) == 0) {
            // Handle the -place=<ime_sekcije>@<adresa> option
            std::string placeArg = arg.substr(7);  // Skip "-place="
            size_t atPos = placeArg.find('@');
            if (atPos != std::string::npos) {
                SectionPlacement sp;
                sp.sectionName = placeArg.substr(0, atPos);
                sp.address = placeArg.substr(atPos + 1);
                sectionPlacements.push_back(sp);
            }
        } else if (arg == "-hex") {
            // Handle the -hex option
            hexFlag = true;
        } else if (arg == "-relocatable") {
            // Handle the -relocatable option
            relocatableFlag = true;
        } else if (arg[0] != '-') {
            // If the argument doesn't start with "-", treat it as an input file
            inputFiles.push_back(arg);
        }
    }

    // Check if exactly one of -hex or -relocatable is set
    if (hexFlag && relocatableFlag) {
        std::cerr << "Linker: Error -> Both -hex and -relocatable cannot be specified together." << std::endl;
        return 1;
    } else if (!hexFlag && !relocatableFlag) {
        std::cerr << "Linker: Error -> You must specify exactly one of -hex or -relocatable." << std::endl;
        return 1;
    }

    // Print out the results for debugging purposes
    if (!outputFile.empty()) {
        std::cout << "Linker: Output file: " << outputFile << std::endl;
    }

    for (const auto &sp : sectionPlacements) {
        std::cout << "Linker: Section: " << sp.sectionName << " at address: " << sp.address << std::endl;
    }

    if (hexFlag) {
        std::cout << "Linker: Hex flag is set." << std::endl;
    }

    if (relocatableFlag) {
        std::cout << "Linker: Relocatable flag is set." << std::endl;
    }

    vector<pair<string, uint32_t>> places;

    for (SectionPlacement sp: sectionPlacements){
        places.push_back(pair<string,uint32_t>(sp.sectionName, literal_to_int_main(sp.address)));
    }
    
    cout << "Linker: Input Files: "; 
    for(string inputFile: inputFiles){
        cout << inputFile << " ";
    }
    cout << endl;

    std::sort(places.begin(), places.end(), 
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return (unsigned int)a.second < (unsigned int)b.second;  // Sort in ascending order
            });
    

    Linker linker(inputFiles, places, outputFile, hexFlag, relocatableFlag);

    linker.startLinking();
    
    return 0;
}