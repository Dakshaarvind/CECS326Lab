/*
 * barbarian.c: the barbarian process for the dungeon game
 * The barbarian's job is simple: when the dungeon sends DUNGEON_SIGNAL,
 * read the enemy's health value and copy it directly into the attack field.
 * The dungeon checks if they match to score the round.
 * On SEMAPHORE_SIGNAL, the barbarian holds lever one open so the rogue
 * can get into the treasure room.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "dungeon_info.h"

// Global pointer to shared memory - set once in main, used by signal handlers
static struct Dungeon *dungeon = NULL;

// DUNGEON_SIGNAL handler: read the enemy health and match it with our attack
static void handle_dungeon_signal(int sig)
{
    (void)sig; // suppress unused parameter warning
    if (dungeon == NULL) return;

    // The dungeon checks if barbarian.attack == enemy.health to count a hit
    dungeon->barbarian.attack = dungeon->enemy.health;
    printf("[barbarian] Attacked! health=%d  attack=%d\n",
           dungeon->enemy.health, dungeon->barbarian.attack);
}

// SEMAPHORE_SIGNAL handler: hold lever one so the rogue can get the treasure
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[barbarian] Holding lever one...\n");

    // Open the named semaphore that was created in game.c
    sem_t *lever = sem_open(dungeon_lever_one, 0);
    if (lever == SEM_FAILED) {
        perror("[barbarian] sem_open lever_one");
        return;
    }

    // Posting increments the semaphore value, signaling the lever is held
    sem_post(lever);

    // Keep holding until the rogue has copied all 4 treasure characters into spoils
    while (dungeon->running) {
        if (dungeon->spoils[0] != '\0' &&
            dungeon->spoils[1] != '\0' &&
            dungeon->spoils[2] != '\0' &&
            dungeon->spoils[3] != '\0') {
            break; // rogue is done, safe to release
        }
        usleep(5000); // check every 5ms so we don't busy-wait
    }

    // Post again to release the door so the dungeon can continue
    sem_post(lever);
    sem_close(lever);
    printf("[barbarian] Released lever one.\n");
}

int main(void)
{
    // Open the shared memory segment - game.c already created and sized it
    int shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[barbarian] shm_open");
        exit(EXIT_FAILURE);
    }

    // Map shared memory into our address space
    dungeon = mmap(NULL, sizeof(struct Dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("[barbarian] mmap");
        exit(EXIT_FAILURE);
    }
    close(shm_fd); // fd not needed once mmap is set up

    printf("[barbarian] Ready. PID=%d\n", getpid());

    // Register DUNGEON_SIGNAL handler using sigaction instead of signal()
    // SA_RESTART makes interrupted system calls (like sleep) resume automatically
    struct sigaction sa_dungeon = {0};
    sa_dungeon.sa_handler = handle_dungeon_signal;
    sa_dungeon.sa_flags   = SA_RESTART;
    sigemptyset(&sa_dungeon.sa_mask);
    sigaction(DUNGEON_SIGNAL, &sa_dungeon, NULL);

    // Register SEMAPHORE_SIGNAL handler the same way
    struct sigaction sa_sem = {0};
    sa_sem.sa_handler = handle_semaphore_signal;
    sa_sem.sa_flags   = SA_RESTART;
    sigemptyset(&sa_sem.sa_mask);
    sigaction(SEMAPHORE_SIGNAL, &sa_sem, NULL);

    // Stay alive until the dungeon sets running to false
    while (dungeon->running) {
        sleep(1);
    }

    printf("[barbarian] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
