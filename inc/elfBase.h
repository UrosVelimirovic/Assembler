/**
 * ELF structure:
 * 
 * ELF header
 * ------------
 * Section header
 * ------------
 * shstrtab section data
 * ------------
 * strtab section data
 * ------------
 * symbol table data
 * ------------
 * section1 machine code data
 * ------------
 * section1 rela data
 * ------------
 * section2 machine code data
 * ------------
 * section2 rela data
 * ------------
 * ...
 * 
 */


typedef int Elf32_Word;
typedef unsigned char Elf32_Byte;

#define EI_NIDENT 16

#define ET_NONE 0 /* No file type */
#define ET_REL 1 /* Relocatable file */
#define ET_EXEC 2 /* Executable file */

#define EM_SSEMU_32 1 // our emulator.

// sht types
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_MACHINECODE 5

#define ELF_HEADER_SIZE (sizeof(Elf32_Byte) * EI_NIDENT + 13 * sizeof(Elf32_Word))
#define SECTION_HEADER_ENTRY_SIZE 10 * sizeof(Elf32_Word)
#define RELA_TABLE_ENTRY_SIZE 3* sizeof(Elf32_Word)
#define SYMTAB_ENTRY_SIZE (2*sizeof(Elf32_Byte) + 4*sizeof(Elf32_Word))
#define SHSTRTAB_INDEX 0
#define STRTAB_INDEX 1
#define SYMBOLTAB_INDEX 2

struct Elf32_EhdrStruct{
    Elf32_Byte e_ident[EI_NIDENT]; 
    Elf32_Word e_type;
    Elf32_Word e_machine;
    Elf32_Word e_version;
    Elf32_Word e_entry;
    Elf32_Word e_phoff;
    Elf32_Word e_shoff;
    Elf32_Word e_flags;
    Elf32_Word e_ehsize;
    Elf32_Word e_phentsize;
    Elf32_Word e_phnum;
    Elf32_Word e_shentsize;
    Elf32_Word e_shnum;
    Elf32_Word e_shstrndx;
};
typedef Elf32_EhdrStruct Elf32_Ehdr;

struct Elf32_ShdrStruct{
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Word sh_addr; // zaboravi na ovo
    Elf32_Word sh_offset; // gde se nalazi sekcija od pocetka fajla
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
};
typedef Elf32_ShdrStruct Elf32_Shdr;

#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define ELF32_ST_BIND(val) (((Elf32_Byte) (val)) >> 4) // getter
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define ELF32_ST_TYPE(val) ((val) & 0xf) // getter
#define STT_NOTYPE 0
#define STT_SECTION 1
struct Elf32_SymStruct{
    Elf32_Word st_name;
    Elf32_Byte st_info; // bind
    Elf32_Byte st_other;
    Elf32_Word st_shndx;
    Elf32_Word st_value;
    Elf32_Word st_size;
};
typedef Elf32_SymStruct Elf32_Sym;

struct Elf32_RelaStruct{
    Elf32_Word r_offset;
    Elf32_Word r_info;
    Elf32_Word r_addend;
};
typedef Elf32_RelaStruct Elf32_Rela;