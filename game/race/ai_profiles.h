/* The game's own driver letters and profiles (004c2850..), from the exe. */
#ifndef TNFS_AI_PROFILES_H
#define TNFS_AI_PROFILES_H


#include <stdint.h>

enum {
    AI_DRIVER_LETTERS = 0x38,             /* 7 rows of 8 */
    AI_DRIVER_PROFILES = 9,
    AI_DRIVER_PROFILE_SIZE = 0x8d,
    AI_DRIVER_DATA_SIZE = AI_DRIVER_LETTERS + AI_DRIVER_PROFILES * AI_DRIVER_PROFILE_SIZE,
    AI_DRIVER_PROFILES_ADDR = 0x4c2888    /* where the profiles sit in the original */
};

extern uint8_t AI_DRIVER_DATA[AI_DRIVER_DATA_SIZE];

#endif
