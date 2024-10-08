assembler_:
	flex misc/lexer.l
	bison -d misc/parser.y
	gcc -g -o assembler lexer.c parser.c ./src/Helper.cpp ./src/Assembler.cpp  -lfl -lstdc++

linker_:
	gcc -g -o linker ./src/Linker.cpp -lfl -lstdc++

emulator_:
	gcc -g -o emulator ./src/Emulator.cpp -lfl -lstdc++ -pthread

clean:
	rm -f linker assembler emulator parser.c parser.h lexer.c lexer.h *.o *.hex

clean_build_run: clean assembler_ linker_ emulator_

	#testA
	# ./assembler -o main.o tests/public-tests/nivo-a/main.s
	# ./assembler -o math.o tests/public-tests/nivo-a/math.s
	# ./assembler -o handler.o tests/public-tests/nivo-a/handler.s
	# ./assembler -o isr_timer.o tests/public-tests/nivo-a/isr_timer.s
	# ./assembler -o isr_terminal.o tests/public-tests/nivo-a/isr_terminal.s
	# ./assembler -o isr_software.o tests/public-tests/nivo-a/isr_software.s
	# ./linker -hex -place=my_code@0x40000000 -place=math@0xF0000000 -o program.hex handler.o math.o main.o isr_terminal.o isr_timer.o isr_software.o
	# ./emulator program.hex

	#testb
	./assembler -o main.o tests/public-tests/nivo-b/main.s
	./assembler -o handler.o tests/public-tests/nivo-b/handler.s
	./assembler -o isr_timer.o tests/public-tests/nivo-b/isr_timer.s
	./assembler -o isr_terminal.o tests/public-tests/nivo-b/isr_terminal.s
	./linker -hex -place=my_code@0x40000000 -o program.hex main.o isr_terminal.o isr_timer.o handler.o
	./emulator program.hex