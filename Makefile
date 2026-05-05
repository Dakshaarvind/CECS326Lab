# Makefile for CECS 326 Lab 2
# Detects OS: Linux needs -lrt for shm_open/sem_open, macOS doesn't

CC     = gcc
CFLAGS = -Wall -Wextra -g

# -lrt is part of glibc on Linux but built into the standard lib on macOS
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
    LIBS = -lrt -pthread
else
    LIBS = -pthread
endif

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
