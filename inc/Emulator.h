#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#define EXECUTE_START_ADDRESS 0x40000000
#define STATUS_REG_INDEX 0
#define HANDLER_REG_INDEX 1
#define CAUSE_REG_INDEX 2
#define MEMORY_MAPPED_REGISTER_START_ADDRESS 0xFFFFFF00
#define TERM_OUT_REG_ADDRESS 0xFFFFFF00
#define TERM_IN_REG_ADDRESS 0xFFFFFF04
#define BYTE_0 0
#define BYTE_1 1
#define BYTE_2 2
#define BYTE_3 3
#define WORD_SIZE 4

#define TIMER_BIT 0 // Tr (Timer) - maskiranje prekida od tajmera (0 - omogućen, 1 - maskiran)
#define TERMINAL_BIT 1 // Tl (Terminal) - maskiranje prekida od terminala (0 - omogućen, 1 - maskiran) 
#define INTERRUPT_BIT 2 // I (Interrupt) - globalno maskiranje spoljašnjih prekida (0 - omogućeni, 1 - maskirani)

#define PC_INDEX 15
#define SP_INDEX 14

#define SP_DEFAULT_VALUE 0x20000000
class Emulator{
private:

    // terminal 
    std::thread terminal;
    bool ready;
    std::mutex mtx;                      // Mutex for synchronizing access
    std::condition_variable cv;        // Condition variable
    std::atomic<bool> stopFlag;


    std::string inputFileName;
    std::map<int, unsigned char> memory;

    uint32_t gprx[16];
    uint32_t csr[3];

    struct InstructionStruct{
        int oc;
        int mod;
        int regA;
        int regB;
        int regC;
        int disp;
        int fullInstruction;
    };
    typedef InstructionStruct Instruction;

    Instruction currentInstruction;
public:
    Emulator(std::string inputFileName): inputFileName(inputFileName), ready(true) {
        stopFlag.store(false);
    }

    void powerOn();

private:
    void init_hardware();
    void init_memory();
    void run();
    void decode_and_execute(Instruction instruction);

    void halt();

    void error_print_and_exit(std::string errorMessage);

    uint32_t gprx_get(uint32_t regIndex);
    void gprx_set(uint32_t regIndex, uint32_t value);
    void gprx_set_byte(uint32_t regIndex, uint32_t byteIndex, unsigned char value);

    uint32_t csr_get(uint32_t regIndex);
    void csr_set(uint32_t regIndex, uint32_t value);
    void csr_set_byte(uint32_t regIndex, uint32_t byteIndex, unsigned char value);

    void status_bit_set(uint32_t bitIndex, bool value);
    uint32_t status_bit_get(uint32_t bitIndex);

    void memory_set_word(uint32_t address, uint32_t value);
    uint32_t memory_get_word(uint32_t address);

    void interruption();
    Instruction get_instruction();

    void set_term_out(uint32_t value);
    uint32_t get_term_out();

    void set_term_in(uint32_t value);
    uint32_t get_term_in();

    void terminal_check();

    void print_end_state();

    void disable_echo();
    void enable_echo();
    void terminal_thread_function();

    void my_exit();
};

int main(int argc, char* argv[]) {
    // Check if the correct number of arguments is passed
    if (argc != 2) {
        std::cerr << "Linker: ERROR -> Usage: " << argv[0] << " <filename>\n";
        return 1;  // Return with error code
    }

    // Catch the file name argument
    std::string filename = argv[1];

    Emulator emu(filename);

    emu.powerOn();

    
    return 0;  // Return success code
}