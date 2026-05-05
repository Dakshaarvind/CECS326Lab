/*
 * wizard.c 
 * The wizard decodes a Caesar cipher from the barrier's spell field and
 * writes the decoded message into the wizard's spell field.
 * The first byte of barrier->spell is the shift key (its ASCII value).
 * Only alphabetic characters are shifted; punctuation and spaces pass through.
 * On SEMAPHORE_SIGNAL, the wizard holds lever two so the rogue can grab treasure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "dungeon_info.h"

// Shared memory pointer, assigned in main and used in signal handlers
static struct Dungeon *dungeon = NULL;

/*
 * caesar_decode - decodes a Caesar cipher
 * src[0] is the shift key (raw ASCII value mod 26)
 * src[1..] is the encoded message
 * Uppercase stays uppercase, lowercase stays lowercase, non-alpha unchanged
 */
static void caesar_decode(const char *src, char *dst, int dst_size)
{
    // Nothing to decode if the spell is empty
    if (src == NULL || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    // Take the shift mod 26 so it stays within one alphabet rotation
    int shift = (unsigned char)src[0] % 26;

    int di = 0;
    // Start at index 1 since index 0 is the key, not part of the message
    for (int si = 1; src[si] != '\0' && di < dst_size - 1; si++) {
        char c = src[si];

        if (isupper((unsigned char)c)) {
            // Shift backwards in uppercase range, multiply by 4 to keep positive
            c = (char)(((c - 'A') - shift + 26 * 4) % 26 + 'A');
        } else if (islower((unsigned char)c)) {
            // Same logic for lowercase range
            c = (char)(((c - 'a') - shift + 26 * 4) % 26 + 'a');
        }
        // Non-alphabetic characters like spaces and punctuation copy as-is

        dst[di++] = c;
    }
    dst[di] = '\0'; // null-terminate the output
}

// DUNGEON_SIGNAL handler: decode the barrier spell and store it in wizard->spell
static void handle_dungeon_signal(int sig)
{
    (void)sig;
    if (dungeon == NULL) return;

    caesar_decode(dungeon->barrier.spell,
                  dungeon->wizard.spell,
                  SPELL_BUFFER_SIZE);

    printf("[wizard] Decoded: \"%s\"\n", dungeon->wizard.spell);
}

// SEMAPHORE_SIGNAL handler: hold lever two so the rogue can enter the treasure room
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[wizard] Holding lever two...\n");

    sem_t *lever = sem_open(dungeon_lever_two, 0);
    if (lever == SEM_FAILED) {
        perror("[wizard] sem_open lever_two");
        return;
    }

    // Post to signal that this lever is being held
    sem_post(lever);

    // Wait until the rogue fills all 4 spoils characters before releasing
    while (dungeon->running) {
        if (dungeon->spoils[0] != '\0' &&
            dungeon->spoils[1] != '\0' &&
            dungeon->spoils[2] != '\0' &&
            dungeon->spoils[3] != '\0') {
            break;
        }
        usleep(5000);
    }

    // Release the lever now that the rogue is out
    sem_post(lever);
    sem_close(lever);
    printf("[wizard] Released lever two.\n");
}

int main(void)
{
    // Open the shared memory that game.c already created
    int shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[wizard] shm_open");
        exit(EXIT_FAILURE);
    }

    dungeon = mmap(NULL, sizeof(struct Dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
    if (dungeon == MAP_FAILED) {
        perror("[wizard] mmap");
        exit(EXIT_FAILURE);
    }
    close(shm_fd);

    printf("[wizard] Ready. PID=%d\n", getpid());

    // Set up signal handlers with SA_RESTART to avoid interrupted syscall issues
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

    // Loop until dungeon->running goes false
    while (dungeon->running) {
        sleep(1);
    }

    printf("[wizard] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
