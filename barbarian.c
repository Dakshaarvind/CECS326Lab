/*
 * barbarian.c
 * When DUNGEON_SIGNAL arrives: copy enemy->health into barbarian->attack.
 * The dungeon later checks whether attack == health to score the round.
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

/* Pointer to the shared Dungeon struct, set once in main */
static struct Dungeon *dungeon = NULL;

/* ------------------------------------------------------------------ */
/* Signal handlers                                                      */
/* ------------------------------------------------------------------ */

/* DUNGEON_SIGNAL: read enemy health and write it to barbarian attack */
static void handle_dungeon_signal(int sig)
{
    (void)sig;
    if (dungeon == NULL) return;

    /* Copy health -> attack so the dungeon sees a matching value */
    dungeon->barbarian.attack = dungeon->enemy.health;
    printf("[barbarian] Attacked! health=%d  attack=%d\n",
           dungeon->enemy.health, dungeon->barbarian.attack);
}

/* SEMAPHORE_SIGNAL: hold lever one open while the rogue gets treasure */
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[barbarian] Holding lever one...\n");

    /* Open the already-created semaphore */
    sem_t *lever = sem_open(dungeon_lever_one, 0);
    if (lever == SEM_FAILED) {
        perror("[barbarian] sem_open lever_one");
        return;
    }

    /* Post (increment) to signal that this lever is being held */
    sem_post(lever);

    /* Wait until the rogue has all four spoils characters, then release */
    while (dungeon->running) {
        /* All four spoils bytes filled means the rogue is done */
        if (dungeon->spoils[0] != '\0' &&
            dungeon->spoils[1] != '\0' &&
            dungeon->spoils[2] != '\0' &&
            dungeon->spoils[3] != '\0') {
            break;
        }
        usleep(5000);
    }

    /* Release by posting again so the dungeon can proceed */
    sem_post(lever);
    sem_close(lever);
    printf("[barbarian] Released lever one.\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    /* Open the shared memory segment created by game.c */
    int shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[barbarian] shm_open");
        exit(EXIT_FAILURE);
    }

    dungeon = mmap(NULL, sizeof(struct Dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("[barbarian] mmap");
        exit(EXIT_FAILURE);
    }
    close(shm_fd);   /* fd no longer needed after mmap */

    printf("[barbarian] Ready. PID=%d\n", getpid());

    /* ---------------------------------------------------------------- */
    /* Register signal handlers with SA_RESTART so slow syscalls resume  */
    /* ---------------------------------------------------------------- */

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

    /* ---------------------------------------------------------------- */
    /* Spin until the dungeon marks the game as finished                 */
    /* ---------------------------------------------------------------- */

    while (dungeon->running) {
        sleep(1);
    }

    printf("[barbarian] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
