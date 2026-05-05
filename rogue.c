/*
 * rogue.c
 * The rogue picks locks using binary search and collects treasure using semaphores.
 * On DUNGEON_SIGNAL: guess the hidden pick angle by reading trap->direction
 * after each guess and adjusting the range up or down accordingly.
 * On SEMAPHORE_SIGNAL: wait for both levers to be held, then collect all
 * 4 treasure characters into dungeon->spoils one at a time.
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

// Global shared memory pointer set in main, used by both signal handlers
static struct Dungeon *dungeon = NULL;

// DUNGEON_SIGNAL handler: binary search for the correct pick angle
static void handle_dungeon_signal(int sig)
{
    (void)sig;
    if (dungeon == NULL) return;

    // Start binary search with full range
    float lo  = 0.0f;
    float hi  = (float)MAX_PICK_ANGLE;
    float mid = (lo + hi) / 2.0f;

    // Set 't' as a sentinel so we know when the dungeon has responded
    // to our pick value. Without this it's hard to tell if direction
    // is stale from a previous round.
    dungeon->trap.direction = 't';
    dungeon->rogue.pick     = mid;

    printf("[rogue] Starting binary search...\n");

    while (dungeon->trap.direction != '-' && dungeon->running) {

        // Spin until the dungeon updates direction away from our sentinel
        while (dungeon->trap.direction == 't' && dungeon->running) {
            usleep(TIME_BETWEEN_ROGUE_TICKS);
        }

        char dir = dungeon->trap.direction;
        if (dir == '-') break; // lock is within threshold, we're done

        if (dir == 'd') {
            // Dungeon says go lower, shrink upper bound
            hi = mid;
        } else if (dir == 'u') {
            // Dungeon says go higher, raise lower bound
            lo = mid;
        }

        // New midpoint for next guess
        mid = (lo + hi) / 2.0f;

        // Reset sentinel before writing new pick so we can detect the next reply
        dungeon->trap.direction = 't';
        dungeon->rogue.pick     = mid;

        printf("[rogue] pick=%.4f  dir=%c\n", mid, dir);
    }

    printf("[rogue] Lock picked! Final pick=%.4f\n", dungeon->rogue.pick);
}

/*
 * SEMAPHORE_SIGNAL handler: collect treasure once both levers are held.
 * dungeon->treasure is NOT null-terminated so we read exactly 4 bytes.
 * The dungeon adds them one at a time with pauses, so we wait for each.
 */
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[rogue] Entering treasure room...\n");

    // Open both named semaphores created in game.c
    sem_t *lever_one = sem_open(dungeon_lever_one, 0);
    sem_t *lever_two = sem_open(dungeon_lever_two, 0);

    if (lever_one == SEM_FAILED || lever_two == SEM_FAILED) {
        perror("[rogue] sem_open levers");
        return;
    }

    // Block here until both barbarian and wizard have posted their levers
    sem_wait(lever_one);
    sem_wait(lever_two);

    printf("[rogue] Door is open, collecting treasure...\n");

    // Read each treasure character as it appears - dungeon fills them with delays
    for (int i = 0; i < 4; i++) {
        while (dungeon->treasure[i] == '\0' && dungeon->running) {
            usleep(5000); // poll every 5ms waiting for next character
        }
        dungeon->spoils[i] = dungeon->treasure[i];
        printf("[rogue] Spoil[%d] = '%c'\n", i, dungeon->spoils[i]);
    }

    printf("[rogue] All treasure collected: %.4s\n", dungeon->spoils);

    // Barbarian and wizard will see spoils is full and release their levers
    sem_close(lever_one);
    sem_close(lever_two);
}

int main(void)
{
    // Open the shared memory segment created by game.c
    int shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[rogue] shm_open");
        exit(EXIT_FAILURE);
    }

    dungeon = mmap(NULL, sizeof(struct Dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("[rogue] mmap");
        exit(EXIT_FAILURE);
    }
    close(shm_fd);

    printf("[rogue] Ready. PID=%d\n", getpid());

    // SA_RESTART ensures sleep/usleep resume after signal is handled
    struct sigaction sa_dungeon = {0};
    sa_dungeon.sa_handler = handle_dungeon_signal;
    sa_dungeon.sa_flags   = SA_RESTART;
    sigemptyset(&sa_dungeon.sa_mask);
    sigaction(DUNGEON_SIGNAL, &sa_dungeon, NULL);

    struct sigaction sa_sem = {0};
    sa_sem.sa_handler = handle_semaphore_signal;
    sa_sem.sa_flags   = SA_RESTART;
    sigemptyset(&sa_sem.sa_mask);
    sigaction(SEMAPHORE_SIGNAL, &sa_sem, NULL);

    // Stay alive until the dungeon finishes
    while (dungeon->running) {
        sleep(1);
    }

    printf("[rogue] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
