#include "./../inc/Emulator.h"

void Emulator::powerOn()
{
    // init hardware
    init_hardware();

    // init memory
    init_memory();

    // run
    run();
}

void Emulator::init_hardware()
{
    // gpr
    for(int i = 0; i < 14; i ++){
        gprx[i] = 0;
    }
    
    // set pc
    gprx[PC_INDEX] = EXECUTE_START_ADDRESS;
   
    // set sp
    gprx[SP_INDEX] = SP_DEFAULT_VALUE;

    // csr
    for(int i = 0; i < 3; i ++){
        csr[i] = 0;
    }
 
    // other registers.
    for(int i = 0; i < 256; i += 4){
        memory[MEMORY_MAPPED_REGISTER_START_ADDRESS + i] = 0;
        memory[MEMORY_MAPPED_REGISTER_START_ADDRESS + i + 1] = 0;
        memory[MEMORY_MAPPED_REGISTER_START_ADDRESS + i + 2] = 0;
        memory[MEMORY_MAPPED_REGISTER_START_ADDRESS + i + 3] = 0;
    }

    
}

void Emulator::init_memory()
{
    std::ifstream file(inputFileName);

    if (!file.is_open()) {
        std::cerr << "Emulator: ERROR -> Could not open file " << inputFileName << "\n";
        my_exit();
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        unsigned int  key;
        std::string hexValue;

        // Parse the key (integer before ':')
        if (iss >> key && std::getline(iss, hexValue, ':')) {
            // Trim spaces before extracting the value
            iss >> hexValue;  // Get the hex value after the colon

            // Convert hex string to unsigned char
            unsigned char value = static_cast<unsigned char>(std::stoul(hexValue, nullptr, 16));

            // Insert key-value pair into the map
            memory[key] = value;
        }
    }

    file.close();

    // for(const auto& pair: memory){
    //     std::cout << pair.first << " : " << std::hex << (int)(pair.second) << std::endl;
    // }
}

void Emulator::run()
{
    terminal = std::thread(&Emulator::terminal_thread_function, this);
    while(true){
        terminal_check();
        currentInstruction = get_instruction();
        //std::cout << gprx_get(1) << std::endl;
        decode_and_execute(currentInstruction);
    }
}

void Emulator::decode_and_execute(Instruction instruction)
{
    int temp;
    // std::cout << instruction.fullInstruction << std::endl;
    switch(instruction.oc){
       
        case 0b0000: // Instrukcija za zaustavljanje procesora
            if(instruction.mod == 0 && instruction.regA == 0
                && instruction.regB == 0 && instruction.regC == 0 && instruction.disp == 0)
            {
                halt();
            }
            else{
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0001: // Instrukcija softverskog prekida
            //std::cout << "E" << std::hex << csr_get(HANDLER_REG_INDEX) << std::endl;
            if(instruction.mod == 0 && instruction.regA == 0
                && instruction.regB == 0 && instruction.regC == 0 && instruction.disp == 0)
            {
                // push status
                gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
                memory_set_word(gprx_get(SP_INDEX), gprx_get(STATUS_REG_INDEX));

                // push pc
                gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
                memory_set_word(gprx_get(SP_INDEX), gprx_get(PC_INDEX));

                // cause <= 4
                csr_set(CAUSE_REG_INDEX, 4);

                // status<=status&(~0x1);
                csr_set(STATUS_REG_INDEX, csr_get(STATUS_REG_INDEX) & (~0x1));
                
                // call interruption, but it does nothing since this is a software interrupt.
                interruption();
            }
            else{
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0010: // Instrukcija poziva potprograma
            if(instruction.regC != 0){
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }

            if(instruction.mod == 0b0000){
                // push pc 
                gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
                memory_set_word(gprx_get(SP_INDEX), gprx_get(PC_INDEX));

                // pc<=gpr[A]+gpr[B]+D;
                gprx_set(
                    PC_INDEX, 
                    gprx_get(instruction.regA) + gprx_get(instruction.regB) + instruction.disp
                );
            } else if (instruction.mod == 0b0001){
                // push pc 
                gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
                memory_set_word(gprx_get(SP_INDEX), gprx_get(PC_INDEX));

                // pc<=mem32[gpr[A]+gpr[B]+D];
                gprx_set(
                    PC_INDEX,
                    memory_get_word(
                        gprx_get(instruction.regA) + gprx_get(instruction.regB) + instruction.disp
                    )
                );
            } else{
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0011: // Instrukcija skoka
            if(instruction.mod == 0b0000){
                // pc<=gpr[A]+D;
                gprx_set(PC_INDEX, gprx_get(instruction.regA) + instruction.disp);
            } else if(instruction.mod == 0b0001){
                // if (gpr[B] == gpr[C]) pc<=gpr[A]+D;
                if(gprx_get(instruction.regB) == gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, gprx_get(instruction.regA) + instruction.disp);
                }
            } else if(instruction.mod == 0b0010){
                // if (gpr[B] != gpr[C]) pc<=gpr[A]+D;
                if(gprx_get(instruction.regB) != gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, gprx_get(instruction.regA) + instruction.disp);
                }
            } else if(instruction.mod == 0b0011){
                // if (gpr[B] signed> gpr[C]) pc<=gpr[A]+D;
                if(gprx_get(instruction.regB) > gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, gprx_get(instruction.regA) + instruction.disp);
                }
            } else if(instruction.mod == 0b1000){
                // pc<=mem32[gpr[A]+D];
                gprx_set(PC_INDEX, memory_get_word(gprx_get(instruction.regA) + instruction.disp));
            } else if(instruction.mod == 0b1001){
                // if (gpr[B] == gpr[C]) pc<=mem32[gpr[A]+D];
                if(gprx_get(instruction.regB) == gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, memory_get_word(gprx_get(instruction.regA) + instruction.disp));
                    
                }
            } else if(instruction.mod == 0b1010){
                // if (gpr[B] != gpr[C]) pc<=mem32[gpr[A]+D];
                if(gprx_get(instruction.regB) != gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, memory_get_word(gprx_get(instruction.regA) + instruction.disp));
                }
            } else if(instruction.mod == 0b1011){
                // if (gpr[B] signed> gpr[C]) pc<=mem32[gpr[A]+D];
                if(gprx_get(instruction.regB) > gprx_get(instruction.regC)){
                    gprx_set(PC_INDEX, memory_get_word(gprx_get(instruction.regA) + instruction.disp));
                }
            } else {
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0100: // Instrukcija atomične zamene vrednosti
            if(instruction.mod != 0 || instruction.regA != 0 || instruction.disp != 0){
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }

            // temp<=gpr[B];
            temp = gprx_get(instruction.regB);

            // gpr[B]<=gpr[C];
            gprx_set(instruction.regB, gprx_get(instruction.regC));

            // gpr[C]<=temp;
            gprx_set(instruction.regC, temp);
            break;
        case 0b0101: // Instrukcija aritmetičkih operacija
            if(instruction.disp != 0){
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }

            if( instruction.mod == 0b0000){
                // gpr[A]<=gpr[B] + gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) + gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0001){
                // gpr[A]<=gpr[B] - gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) - gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0010){
                // gpr[A]<=gpr[B] * gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) * gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0011){
                // gpr[A]<=gpr[B] / gpr[C];
               // std::cout << gprx_get(instruction.regB) << " " << gprx_get(instruction.regC) << std::endl;
                gprx_set(instruction.regA, gprx_get(instruction.regB) / gprx_get(instruction.regC));
            } else {
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0110: // Instrukcija logičkih operacija
            if(instruction.disp != 0){
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }

            if( instruction.mod == 0b0000){
                // gpr[A]<=~gpr[B];
                gprx_set(instruction.regA, ~(gprx_get(instruction.regB)) );
            } else if(instruction.mod == 0b0001){
                // gpr[A]<=gpr[B] & gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) & gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0010){
                // gpr[A]<=gpr[B] | gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) | gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0011){
                // gpr[A]<=gpr[B] ^ gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) ^ gprx_get(instruction.regC));
            } else {
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b0111: // Instrukcija pomeračkih operacija
            if(instruction.disp != 0){
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }

            if(instruction.mod == 0b0000){
                //gpr[A]<=gpr[B] << gpr[C];
                gprx_set(instruction.regA, gprx_get(instruction.regB) << gprx_get(instruction.regC));
            } else if(instruction.mod == 0b0001){
                //  gpr[A]<=gpr[B] >> gpr[C];
                gprx_set(instruction.regA, (uint32_t)gprx_get(instruction.regB) >> gprx_get(instruction.regC));
            } else{
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b1000: // Instrukcija smeštanja podatka
            if(instruction.mod == 0b0000){
                // mem32[gpr[A]+gpr[B]+D]<=gpr[C];
                memory_set_word(
                    gprx_get(instruction.regA) + gprx_get(instruction.regB) + instruction.disp,
                    gprx_get(instruction.regC)
                );
            } else if(instruction.mod == 0b0010){
                // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
                memory_set_word(
                    memory_get_word(
                        gprx_get(instruction.regA) + gprx_get(instruction.regB) + instruction.disp
                    ),
                    gprx_get(instruction.regC)
                );
            } else if( instruction.mod == 0b0001){
                // gpr[A]<=gpr[A]+D; mem32[gpr[A]]<=gpr[C];
                // std::cout << instruction.disp << std::endl;
                // std::cout << gprx_get(instruction.regC) << std::endl;

                gprx_set(instruction.regA, gprx_get(instruction.regA) + instruction.disp);
                memory_set_word(gprx_get(instruction.regA), gprx_get(instruction.regC));

            } else{
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        case 0b1001: // Instrukcija učitavanja podatka
            if(instruction.mod == 0b0000){
                // gpr[A]<=csr[B];
                gprx_set(instruction.regA, csr_get(instruction.regB));
            } else if(instruction.mod == 0b0001){
                // gpr[A]<=gpr[B]+D;
                gprx_set(instruction.regA, gprx_get(instruction.regB) + instruction.disp);
            } else if(instruction.mod == 0b0010){ 
                //  gpr[A]<=mem32[gpr[B]+gpr[C]+D];
                //std::cout << "E E " << instruction.regB << " " << instruction.regC << " " << instruction.disp << std::endl;
                gprx_set(
                    instruction.regA,
                    memory_get_word(
                        gprx_get(instruction.regB) + gprx_get(instruction.regC) + instruction.disp
                    )
                );
            } else if(instruction.mod == 0b0011){
                // gpr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
                gprx_set(
                    instruction.regA,
                    memory_get_word(gprx_get(instruction.regB))
                );
               
                gprx_set(instruction.regB, gprx_get(instruction.regB) + instruction.disp);
            } else if(instruction.mod == 0b0100){
                // csr[A]<=gpr[B];
                csr_set(instruction.regA, gprx_get(instruction.regB));
            } else if(instruction.mod == 0b0101){
                // csr[A]<=csr[B]|D;
                csr_set(instruction.regA, csr_get(instruction.regB) | instruction.disp);
            } else if(instruction.mod == 0b0110){
                // csr[A]<=mem32[gpr[B]+gpr[C]+D];
                csr_set(
                    instruction.regA, 
                    memory_get_word(
                        gprx_get(instruction.regB) + gprx_get(instruction.regC) + instruction.disp
                    )
                );
            } else if(instruction.mod == 0b0111){
                // csr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
                csr_set(
                    instruction.regA, 
                    memory_get_word(gprx_get(instruction.regB))
                );
                gprx_set(instruction.regB, gprx_get(instruction.regB) + instruction.disp);
            } else {
                csr_set(CAUSE_REG_INDEX, 1);
                interruption();
            }
            break;
        default:
            csr_set(CAUSE_REG_INDEX, 1);
            interruption();
            break;
    }
}

void Emulator::halt()
{
    print_end_state();
    my_exit();
}

void Emulator::error_print_and_exit(std::string errorMessage)
{
    std::cout << errorMessage << std::endl;
    my_exit();
}

uint32_t Emulator::gprx_get(uint32_t regIndex)
{
    if(regIndex < 0 || regIndex > 15){
        error_print_and_exit("Emulator: ERROR -> gprx_get gprx index " + std::to_string(regIndex) + " out of bounds" );
    }
 
    return gprx[regIndex];
}

void Emulator::gprx_set(uint32_t regIndex, uint32_t value)
{
    if(regIndex < 0 || regIndex > 15){
        error_print_and_exit("Emulator: ERROR -> gprx_set gprx index " + std::to_string(regIndex) + " out of bounds" );
    }

    gprx[regIndex] = value;
}

void Emulator::gprx_set_byte(uint32_t regIndex, uint32_t byteIndex, unsigned char value)
{
    if(regIndex < 0 || regIndex > 15){
        error_print_and_exit("Emulator: ERROR -> gprx_set_byte gprx index " + std::to_string(regIndex) + " out of bounds" );
    }

    if(byteIndex < BYTE_0 || byteIndex > BYTE_3){
        error_print_and_exit("Emulator: ERROR -> gprx_set_byte byte index " + std::to_string(byteIndex) + " out of bounds" );
    }

    gprx[regIndex] = ( gprx[regIndex] & ( ~(0x000000FF << (byteIndex * 8)) ) ) | ( ((int)value) << (byteIndex * 8) );
}

uint32_t Emulator::csr_get(uint32_t regIndex)
{
    if(regIndex < 0 || regIndex > 3){
        error_print_and_exit("Emulator: ERROR -> csr_get csr index " + std::to_string(regIndex) + " out of bounds" );
    }

    return csr[regIndex];
}

void Emulator::csr_set(uint32_t regIndex, uint32_t value)
{
    if(regIndex < 0 || regIndex > 3){
        error_print_and_exit("Emulator: ERROR -> csr_set csr index " + std::to_string(regIndex) + " out of bounds" );
    }

    csr[regIndex] = value;
}

void Emulator::csr_set_byte(uint32_t regIndex, uint32_t byteIndex, unsigned char value)
{
    if(regIndex < 0 || regIndex > 3){
        error_print_and_exit("Emulator: ERROR -> csr_set_byte csr index " + std::to_string(regIndex) + " out of bounds" );
    }

    if(byteIndex < BYTE_0 || byteIndex > BYTE_3){
        error_print_and_exit("Emulator: ERROR -> csr_set_byte byte index " + std::to_string(byteIndex) + " out of bounds" );
    }

    csr[regIndex] = ( csr[regIndex] & ( ~(0x000000FF << (byteIndex * 8)) ) ) | ( ((int)value) << (byteIndex * 8) );
}

void Emulator::status_bit_set(uint32_t bitIndex, bool value)
{
    if(bitIndex < 0 || bitIndex > 31){
        error_print_and_exit("Emulator: ERROR -> status_bit_set bit index " + std::to_string(bitIndex) + " out of bounds" );
    }

    int val = (value == true? 1: 0);

    csr[STATUS_REG_INDEX] = ( csr[STATUS_REG_INDEX] & ( ~(0x00000001 << (bitIndex)) ) )  |  ( (val) << bitIndex );
}

uint32_t Emulator::status_bit_get(uint32_t bitIndex)
{
    if(bitIndex < 0 || bitIndex > 31){
        error_print_and_exit("Emulator: ERROR -> status_bit_get bit index " + std::to_string(bitIndex) + " out of bounds" );
    }

    return (csr[STATUS_REG_INDEX] >> bitIndex) & 0x00000001;
}

void Emulator::memory_set_word(uint32_t address, uint32_t value)
{
    unsigned char byte0 = value;
    unsigned char byte1 = (value >> 8) & 0x000000FF;
    unsigned char byte2 = (value >> 16) & 0x000000FF;
    unsigned char byte3 = (value >> 24) & 0x000000FF;

    memory[address] = byte0;
    memory[address + 1] = byte1;
    memory[address + 2] = byte2;
    memory[address + 3] = byte3;
}
uint32_t Emulator::memory_get_word(uint32_t address)
{
    unsigned char byte0 = memory[address];
    unsigned char byte1 = memory[address + 1];
    unsigned char byte2 = memory[address + 2];
    unsigned char byte3 = memory[address + 3];

    int retValue = 0;

    retValue |= (int)byte0;
    retValue |= ( (int)byte1 << 8 );
    retValue |= ( (int)byte2 << 16 );
    retValue |= ( (int)byte3 << 24 );

    return retValue;
}

void Emulator::interruption()
{
    uint32_t address = csr_get(HANDLER_REG_INDEX);
    uint32_t cause = csr_get(CAUSE_REG_INDEX);

    std::stringstream ss;
    ss << std::hex << currentInstruction.fullInstruction;
    uint8_t ch;
    switch(cause){
        case 1:
            error_print_and_exit("Emulator: ERROR -> bad instruction " + ss.str() + " not covered" );
            break;
        case 2: // timer
            break;
        case 3: // terminal
            //std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEE" << std::endl;
            ch = get_term_in();
            set_term_out((uint32_t)ch);
            break;
        case 4:   
            break;
        default:
            error_print_and_exit("Emulator: ERROR -> interruption cause value " + std::to_string(cause) + " not covered" );
            break;
    }

    // jump
    gprx_set(PC_INDEX, address);
}

Emulator::Instruction Emulator::get_instruction()
{
    unsigned char byte0;
    unsigned char byte1;
    unsigned char byte2;
    unsigned char byte3;

    uint32_t pc = gprx_get(PC_INDEX);
    
    //std::cout << pc << std::endl;
    byte0 = memory[pc];
    byte1 = memory[pc + 1];
    byte2 = memory[pc + 2];
    byte3 = memory[pc + 3];

    uint32_t instruction = 0;
    instruction |= (int)byte0;
    instruction |= ( (int)byte1 << 8 ); 
    instruction |= ( (int)byte2 << 16 ); 
    instruction |= ( (int)byte3 << 24 ); 
    // if (pc == 0xf0000060){
    //     std::cout << std::hex << instruction << std::endl;
    // }
  
    //std::cout << std::hex << uint32_t(pc) << ": " << instruction << std::endl;
    Instruction retInst;


    retInst.oc = (instruction >> 28) & 0x0000000F;
    retInst.mod = ( (instruction << 4) >> 28 ) & 0x0000000F;
    retInst.regA = ( (instruction << 8) >> 28 ) & 0x0000000F;
    retInst.regB = ( (instruction << 12) >> 28 ) & 0x0000000F;
    retInst.regC = ( (instruction << 16) >> 28 ) & 0x0000000F;
    retInst.disp = ( instruction & 0x00000FFF);
    retInst.fullInstruction = instruction;
    //std::cout << retInst.oc << std::endl;
    // sign extend disp
    if(retInst.disp & 0x800){
        retInst.disp |= 0xFFFFF000;
    }

    gprx_set(PC_INDEX, pc + 4);
    //std::cout << gprx_get(PC_INDEX) << std::endl;
    return retInst;
}

void Emulator::set_term_out(uint32_t value)
{
    memory_set_word(TERM_OUT_REG_ADDRESS, value);
}

uint32_t Emulator::get_term_out()
{
    return memory_get_word(TERM_OUT_REG_ADDRESS);
}

void Emulator::set_term_in(uint32_t value)
{
    memory_set_word(TERM_IN_REG_ADDRESS, value);
}

uint32_t Emulator::get_term_in()
{
    return memory_get_word(TERM_IN_REG_ADDRESS);
}

void Emulator::terminal_check()
{
    if(ready == false){
        // push status
        gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
        memory_set_word(gprx_get(SP_INDEX), gprx_get(STATUS_REG_INDEX));

        // push pc
        gprx_set(SP_INDEX, gprx_get(SP_INDEX) - 4);
        memory_set_word(gprx_get(SP_INDEX), gprx_get(PC_INDEX));

        // cause <= 4
        csr_set(CAUSE_REG_INDEX, 3);

        // status<=status&(~0x1);
        csr_set(STATUS_REG_INDEX, csr_get(STATUS_REG_INDEX) & (~0x1));

        interruption();

        ready = true;

        cv.notify_one();
    }
}

void Emulator::print_end_state()
{
    // Print halt message
    std::cout << "-----------------------------------------------------------------" << std::endl;
    std::cout << "Emulated processor executed halt instruction" << std::endl;
    std::cout << "Emulated processor state:" << std::endl;
    
    // Print the registers in rows of 4, each formatted as hex
    for (int i = 0; i < 15; ++i) {
        // Print the register name (r0, r1, etc.) and value in hexadecimal
        std::cout << "r" << std::left << std::dec << i
                  << "=0x" << std::setfill('0') << std::hex << gprx[i] << " ";
        
        // Print a newline after every 4th register, except after r15
        if ((i + 1) % 4 == 0) {
            std::cout << std::endl;
        }
    }
    
    // Print the last register (r15) on a new line
    std::cout << "r15=0x" << std::setfill('0') << std::setw(8) << std::hex << gprx[15] << std::endl;
}

void Emulator::disable_echo()
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);  // Get terminal attributes
    tty.c_lflag &= ~ECHO;           // Disable echo
    tty.c_lflag &= ~ICANON;         // Disable canonical mode (line buffering)
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);  // Set the new attributes immediately
}

void Emulator::enable_echo()
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);  // Get terminal attributes
    tty.c_lflag |= ECHO;            // Enable echo
    tty.c_lflag |= ICANON;          // Enable canonical mode (line buffering)
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);  // Set the new attributes immediately
}

void Emulator::terminal_thread_function()
{
    disable_echo();  // Disable echo and line buffering
    
    char ch;
    while (!stopFlag.load()) {
        // Wait for a single character to be pressed
        ch = getchar();
        if(stopFlag.load()){
            break;
        }
        set_term_in((uint32_t)ch);

        ready = false;
       
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, [this]{ return ready; });
    }

    enable_echo();  // Restore terminal settings

}

void Emulator::my_exit()
{
    stopFlag.store(true);

    ready = true;

    cv.notify_one();
    
    terminal.join();

    my_exit();
}
