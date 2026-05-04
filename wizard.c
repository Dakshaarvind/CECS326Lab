/*
 * wizard.c
 * When DUNGEON_SIGNAL arrives: read barrier->spell, decode the Caesar cipher
 * (first byte is the shift key), write decoded text into wizard->spell.
 *
 * Caesar decode rule (alphabetic characters only, preserving case):
 *   decoded = ((original - base) - shift + 26*4) % 26 + base
 * where base is 'A' for uppercase, 'a' for lowercase.
 * The multiplier 26*4 ensures the modulo argument stays positive.
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

static struct Dungeon *dungeon = NULL;

/* ------------------------------------------------------------------ */
/* Caesar-cipher decoder                                                */
/* ------------------------------------------------------------------ */

/*
 * Decodes src (null-terminated) into dst using shift derived from src[0].
 * src[0] is the raw key byte (its ASCII value is the shift amount).
 * Characters after src[0] that are alphabetic are shifted backwards by that
 * amount, wrapping within their respective case range. Non-alpha characters
 * are copied unchanged.
 */
static void caesar_decode(const char *src, char *dst, int dst_size)
{
    if (src == NULL || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    /* The first byte of barrier->spell is the numeric shift key */
    int shift = (unsigned char)src[0] % 26;

    int di = 0;
    /* Start from index 1 — index 0 is the key, not payload */
    for (int si = 1; src[si] != '\0' && di < dst_size - 1; si++) {
        char c = src[si];
        if (isupper((unsigned char)c)) {
            /* Shift backwards within 'A'..'Z' */
            c = (char)(((c - 'A') - shift + 26 * 4) % 26 + 'A');
        } else if (islower((unsigned char)c)) {
            /* Shift backwards within 'a'..'z' */
            c = (char)(((c - 'a') - shift + 26 * 4) % 26 + 'a');
        }
        /* Non-alpha (spaces, punctuation) are copied as-is */
        dst[di++] = c;
    }
    dst[di] = '\0';
}

/* ------------------------------------------------------------------ */
/* Signal handlers                                                      */
/* ------------------------------------------------------------------ */

static void handle_dungeon_signal(int sig)
{
    (void)sig;
    if (dungeon == NULL) return;

    /* Decode the barrier spell and write it to wizard->spell */
    caesar_decode(dungeon->barrier.spell,
                  dungeon->wizard.spell,
                  SPELL_BUFFER_SIZE);

    printf("[wizard] Decoded: \"%s\"\n", dungeon->wizard.spell);
}

/* SEMAPHORE_SIGNAL: hold lever two open while the rogue collects treasure */
static void handle_semaphore_signal(int sig)
{
    (void)sig;
    printf("[wizard] Holding lever two...\n");

    sem_t *lever = sem_open(dungeon_lever_two, 0);
    if (lever == SEM_FAILED) {
        perror("[wizard] sem_open lever_two");
        return;
    }

    /* Signal that this lever is held */
    sem_post(lever);

    /* Wait for rogue to finish collecting all four spoil characters */
    while (dungeon->running) {
        if (dungeon->spoils[0] != '\0' &&
            dungeon->spoils[1] != '\0' &&
            dungeon->spoils[2] != '\0' &&
            dungeon->spoils[3] != '\0') {
            break;
        }
        usleep(5000);
    }

    /* Release the lever */
    sem_post(lever);
    sem_close(lever);
    printf("[wizard] Released lever two.\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
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

    printf("[wizard] Dungeon ended. Exiting.\n");
    munmap(dungeon, sizeof(struct Dungeon));
    return 0;
}
