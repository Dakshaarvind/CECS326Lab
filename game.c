/*
 * game.c
 * Launcher process: creates shared memory, opens semaphores, forks+execs the
 * three character processes, then calls RunDungeon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include "dungeon_info.h"

int main(void)
{
    /* ------------------------------------------------------------------ */
    /* 1. Create and map shared memory                                      */
    /* ------------------------------------------------------------------ */

    /* shm_open creates (or opens) a POSIX shared memory object */
    int shm_fd = shm_open(dungeon_shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    /* Size the object to hold exactly one Dungeon struct */
    if (ftruncate(shm_fd, sizeof(struct Dungeon)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    /* Map the object into this process's virtual address space */
    struct Dungeon *dungeon = mmap(NULL, sizeof(struct Dungeon),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    /* Zero out the struct so all fields start in a known state */
    memset(dungeon, 0, sizeof(struct Dungeon));
    dungeon->running = true;

    /* ------------------------------------------------------------------ */
    /* 2. Create named semaphores (levers) before RunDungeon is called      */
    /* ------------------------------------------------------------------ */

    /* Remove any stale semaphores from a previous run */
    sem_unlink(dungeon_lever_one);
    sem_unlink(dungeon_lever_two);

    /* Initial value 0 means each lever starts "not held" */
    sem_t *lever_one = sem_open(dungeon_lever_one, O_CREAT | O_EXCL, 0666, 0);
    if (lever_one == SEM_FAILED) {
        perror("sem_open lever_one");
        exit(EXIT_FAILURE);
    }

    sem_t *lever_two = sem_open(dungeon_lever_two, O_CREAT | O_EXCL, 0666, 0);
    if (lever_two == SEM_FAILED) {
        perror("sem_open lever_two");
        exit(EXIT_FAILURE);
    }

    /* ------------------------------------------------------------------ */
    /* 3. Fork and exec the three character processes                       */
    /* ------------------------------------------------------------------ */

    pid_t pid_barbarian = fork();
    if (pid_barbarian == -1) {
        perror("fork barbarian");
        exit(EXIT_FAILURE);
    }
    if (pid_barbarian == 0) {
        /* Child: replace image with the barbarian executable */
        execl("./barbarian", "./barbarian", NULL);
        perror("execl barbarian");   /* only reached on error */
        exit(EXIT_FAILURE);
    }

    pid_t pid_wizard = fork();
    if (pid_wizard == -1) {
        perror("fork wizard");
        exit(EXIT_FAILURE);
    }
    if (pid_wizard == 0) {
        execl("./wizard", "./wizard", NULL);
        perror("execl wizard");
        exit(EXIT_FAILURE);
    }

    pid_t pid_rogue = fork();
    if (pid_rogue == -1) {
        perror("fork rogue");
        exit(EXIT_FAILURE);
    }
    if (pid_rogue == 0) {
        execl("./rogue", "./rogue", NULL);
        perror("execl rogue");
        exit(EXIT_FAILURE);
    }

    printf("[game] Started barbarian=%d  wizard=%d  rogue=%d\n",
           pid_barbarian, pid_wizard, pid_rogue);

    /* Brief pause so child processes can reach their signal handlers
     * before the dungeon starts firing signals */
    sleep(1);

    /* ------------------------------------------------------------------ */
    /* 4. Hand control to the dungeon                                       */
    /* ------------------------------------------------------------------ */

    RunDungeon(pid_wizard, pid_rogue, pid_barbarian);

    /* ------------------------------------------------------------------ */
    /* 5. Wait for children, then clean up shared memory and semaphores     */
    /* ------------------------------------------------------------------ */

    waitpid(pid_barbarian, NULL, 0);
    waitpid(pid_wizard,    NULL, 0);
    waitpid(pid_rogue,     NULL, 0);

    /* Unmap and remove shared memory */
    munmap(dungeon, sizeof(struct Dungeon));
    close(shm_fd);
    shm_unlink(dungeon_shm_name);

    /* Close and remove semaphores */
    sem_close(lever_one);
    sem_close(lever_two);
    sem_unlink(dungeon_lever_one);
    sem_unlink(dungeon_lever_two);

    printf("[game] Dungeon complete. Goodbye.\n");
    return 0;
}
