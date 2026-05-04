/*
 * rogue.c
 * When DUNGEON_SIGNAL arrives: binary-search for the hidden pick angle by
 * reading trap->direction after each guess.
 *
 * Binary search strategy:
 *   lo = 0, hi = MAX_PICK_ANGLE, mid = (lo+hi)/2
 *   Write mid into rogue->pick, then wait for direction != 't':
 *     'd' -> target is lower  -> hi = mid
 *     'u' -> target is higher -> lo = mid
 *     '-' -> found (within LOCK_THRESHOLD), stop
 *
 * SEMAPHORE_SIGNAL: wait until both levers are held, then collect the
 * treasure one byte at a time and copy all four into dungeon->spoils.
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

static struct Dungeon *dungeon = NULL;

/* ------------------------------------------------------------------ */
/* Signal handlers                                                      */
/* ------------------------------------------------------------------ */

static void handle_dungeon_signal(int sig)
{
    (void)sig;
    if (dungeon == NULL) return;

    float lo  = 0.0f;
    float hi  = (float)MAX_PICK_ANGLE;
    float mid = (lo + hi) / 2.0f;

    /* Initialise pick and mark direction so we can detect the first reply */
    dungeon->trap.direction = 't';
    dungeon->rogue.pick     = mid;

    printf("[rogue] Starting binary search...\n");

    /* Loop until the dungeon signals success ('-') or time runs out */
    while (dungeon->trap.direction != '-' && dungeon->running) {

        /* Wait for the dungeon to respond to our latest pick value */
        while (dungeon->trap.direction == 't' && dungeon->running) {
            usleep(TIME_BETWEEN_ROGUE_TICKS);
        }

        char dir = dungeon->trap.direction;
        if (dir == '-') break;          /* lock opened */

        if (dir == 'd') {
            /* Target is below current pick */
            hi = mid;
        } else if (dir == 'u') {
            /* Target is above current pick */
            lo = mid;
        }

        mid = (lo + hi) / 2.0f;

        /* Mark 't' so we can detect when the dungeon has responded again */
        dungeon->trap.direction = 't';
        dungeon->rogue.pick     = mid;

        printf("[rogue] pick=%.4f  dir=%c\n", mid, dir);
    }

    printf("[rogue] Lock picked! Final pick=%.4f\n", dungeon->rogue.pick);
}

/*
 * SEMAPHORE_SIGNAL: collect treasure from dungeon->treasure one character at
 * a time and accumulate in dungeon->spoils. The dungeon fills treasure[0..3]
 * with pauses between each character, and the field is NOT null-terminated,
 * so we track how many bytes we have read manually.
 */
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[rogue] Entering treasure room...\n");

    /* Open both levers; the dungeon opened them for us */
    sem_t *lever_one = sem_open(dungeon_lever_one, 0);
    sem_t *lever_two = sem_open(dungeon_lever_two, 0);

    if (lever_one == SEM_FAILED || lever_two == SEM_FAILED) {
        perror("[rogue] sem_open levers");
        return;
    }

    /* Block until both levers are held (both semaphore values become >= 1) */
    sem_wait(lever_one);
    sem_wait(lever_two);

    printf("[rogue] Door is open, collecting treasure...\n");

    /* Collect all four treasure bytes, waiting for each to appear */
    for (int i = 0; i < 4; i++) {
        /* The dungeon adds characters with pauses; wait for a non-null byte */
        while (dungeon->treasure[i] == '\0' && dungeon->running) {
            usleep(5000);
        }
        dungeon->spoils[i] = dungeon->treasure[i];
        printf("[rogue] Spoil[%d] = '%c'\n", i, dungeon->spoils[i]);
    }

    printf("[rogue] All treasure collected: %.4s\n", dungeon->spoils);

    /* The lever holders watch spoils[] and will post their semaphores
     * once they see all four bytes; just close our handles here. */
    sem_close(lever_one);
    sem_close(lever_two);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
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

    while (dungeon->running) {
        sleep(1);
    }

    printf("[rogue] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
