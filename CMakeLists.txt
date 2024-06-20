cmake_minimum_required(VERSION 3.10)
project(peach C)
add_executable(peach chunk.c compiler.c debug.c main.c memory.c object.c scanner.c table.c value.c vm.c gc.c)