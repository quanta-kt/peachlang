peach:
	gcc -g chunk.c compiler.c debug.c main.c memory.c object.c scanner.c table.c value.c vm.c -o build/peach

all: peach
