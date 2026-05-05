/*
 * game.c 
 * This is the main launcher for the dungeon game. It sets up shared memory,
 * creates the semaphores for the treasure room, then forks and execs the three
 * character processes (barbarian, wizard, rogue) before handing off to RunDungeon.
 * After the dungeon finishes, it cleans everything up.
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
    // --- Step 1: Set up shared memory ---
    // Unlink any stale segment left over from a previous crash before creating
    // a fresh one. Without this, shm_open reuses the old object and memset
    // may not fully reset it if the size changed or the process died mid-write.
    shm_unlink(dungeon_shm_name);

    int shm_fd = shm_open(dungeon_shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // ftruncate sets the size of the shared memory object to fit our Dungeon struct
    if (ftruncate(shm_fd, sizeof(struct Dungeon)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // mmap maps the shared memory into this process's address space so we can
    // use it like a regular pointer. MAP_SHARED means writes are visible to
    // other processes that map the same object.
    struct Dungeon *dungeon = mmap(NULL, sizeof(struct Dungeon),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Zero out the struct so nothing starts with garbage values
    memset(dungeon, 0, sizeof(struct Dungeon));
    dungeon->running = true;

    // --- Step 2: Create semaphores before calling RunDungeon ---
    // The dungeon expects both levers to exist by the time it runs.
    // sem_unlink removes any leftover semaphores from a previous crashed run.
    sem_unlink(dungeon_lever_one);
    sem_unlink(dungeon_lever_two);

    // Initial value of 0 means the levers start "not held" —
    // the barbarian and wizard will post to them when they receive SEMAPHORE_SIGNAL
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

    // --- Step 3: Fork and exec each character process ---
    // fork() creates a child process that is a copy of this one.
    // If fork returns 0, we're in the child — exec replaces the child's image
    // with the character executable. The parent gets back the child's PID.

    pid_t pid_barbarian = fork();
    if (pid_barbarian == -1) {
        perror("fork barbarian");
        exit(EXIT_FAILURE);
    }
    if (pid_barbarian == 0) {
        execl("./barbarian", "./barbarian", NULL);
        perror("execl barbarian"); // only gets here if exec failed
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

    // Give the children a moment to set up their signal handlers before
    // the dungeon starts sending DUNGEON_SIGNAL
    sleep(1);

    // --- Step 4: Run the dungeon ---
    // This blocks until the dungeon finishes. All the action happens here.
    RunDungeon(pid_wizard, pid_rogue, pid_barbarian);

    // --- Step 5: Wait for children to exit, then clean up ---
    // waitpid prevents zombie processes by collecting each child's exit status
    waitpid(pid_barbarian, NULL, 0);
    waitpid(pid_wizard,    NULL, 0);
    waitpid(pid_rogue,     NULL, 0);

    // Unmap and delete the shared memory segment
    munmap(dungeon, sizeof(struct Dungeon));
    close(shm_fd);
    shm_unlink(dungeon_shm_name);

    // Close our handles and remove the named semaphores
    sem_close(lever_one);
    sem_close(lever_two);
    sem_unlink(dungeon_lever_one);
    sem_unlink(dungeon_lever_two);

    printf("[game] Dungeon complete. Goodbye.\n");
    return 0;
}
