# Makefile for CECS 326 Lab 2
# Build all four executables and link against dungeon.o
# macOS: shm_open/sem_open are in the standard library; no -lrt needed

CC     = gcc
CFLAGS = -Wall -Wextra -g
LIBS   = -pthread

# All targets built by default
all: game barbarian wizard rogue

game: game.c dungeon_info.h dungeon_settings.h dungeon.o
	$(CC) $(CFLAGS) game.c dungeon.o -o game $(LIBS)

barbarian: barbarian.c dungeon_info.h dungeon_settings.h
	$(CC) $(CFLAGS) barbarian.c -o barbarian $(LIBS)

wizard: wizard.c dungeon_info.h dungeon_settings.h
	$(CC) $(CFLAGS) wizard.c -o wizard $(LIBS)

rogue: rogue.c dungeon_info.h dungeon_settings.h
	$(CC) $(CFLAGS) rogue.c -o rogue $(LIBS)

clean:
	rm -f game barbarian wizard rogue
