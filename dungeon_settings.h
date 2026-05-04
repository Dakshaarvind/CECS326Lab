#ifndef DUNGEON_SETTINGS_H
#define DUNGEON_SETTINGS_H
#include <signal.h>

/* Maximum characters in the spell buffer */
#define SPELL_BUFFER_SIZE (100)

/* Minimum dungeon rounds and points per round */
#define NUM_ROUNDS (10)
#define POINTS_PER_ROUND (5)

/* Seconds the rogue has to pick the lock */
#define SECONDS_TO_PICK (4)

/* Seconds the wizard has to decode the barrier spell */
#define SECONDS_TO_GUESS_BARRIER (2)

/* Seconds the barbarian has to land an attack */
#define SECONDS_TO_ATTACK (2)

/* Floating-point tolerance for a successful lock pick */
#define LOCK_THRESHOLD (2.5)

/* Enable/disable each character class */
#define ALLOW_BARBARIAN true
#define ALLOW_ROGUE     true
#define ALLOW_WIZARD    true

/* Microseconds between dungeon checks of rogue->pick */
#define TIME_BETWEEN_ROGUE_TICKS (10000)

/* Upper bound of the random pick angle */
#define MAX_PICK_ANGLE (100)

/* Signal used by the dungeon to trigger character actions */
#define DUNGEON_SIGNAL   (SIGUSR1)

/* Signal used by the dungeon to trigger semaphore actions */
#define SEMAPHORE_SIGNAL (SIGUSR2)

/* Minimum guaranteed runs per character class */
#define MIN_BARBARIAN_RUNS (2)
#define MIN_ROGUE_RUNS     (2)
#define MIN_WIZARD_RUNS    (2)

/* Seconds the treasure door stays open automatically */
#define TIME_TREASURE_AVAILABLE (10)

/* Scoring constants */
#define POINTS_PER_TREASURE_CHAR     (55)
#define POINTS_FOR_POSTING_SEMAPHORES (0)
#define POINTS_FOR_SEMAPHORES (POINTS_PER_TREASURE_CHAR * 4 + POINTS_FOR_POSTING_SEMAPHORES)

#endif
