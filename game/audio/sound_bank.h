/* The sound driver's banks (the .BNK files): 0048ed30 registers a bank's
 * sounds by number, 0048edfc takes them back. */
#ifndef TNFS_SOUND_BANK_H
#define TNFS_SOUND_BANK_H

#include <stddef.h>
#include <stdint.h>

#include "game/audio/sound_driver.h"

enum {
    SOUND_BANK_SOUNDS = 128,      /* the file's first 0x200 bytes: an offset per number, 0 none */
    SOUND_BANK_RECORD = 0x20,     /* the record (+4 the header's offset) */
    SOUND_BANK_HEADER = 0x20,     /* the EACS header (+0x18 the data's offset) */
    SOUND_BANK_TAKEN = -17,
    SOUND_BANK_NONE = -6
};

/* A registered bank: its records and headers read from the file. */
typedef struct SoundBank {
    SoundBankSound sound[SOUND_BANK_SOUNDS];
    SoundSample sample[SOUND_BANK_SOUNDS];
} SoundBank;

/* 0048ed30 (eax the file in memory): the first free of the 5 bank entries
 * 004c79a0 (none: -17); each number with an offset takes its record (the
 * registry 0052eed4; a number already taken: -17, the numbers before it
 * staying registered) with its header (+4) and data (the header's +0x18),
 * which the original relocates in place by the file's address and the
 * port reads into `bank`; the entry set and returned. A NULL file: -6.
 * The port refuses (-6) a file whose offsets run past `size`, where the
 * original reads past it; no CD bank shares a record or header between
 * numbers, which the original would relocate twice. */
int32_t sound_bank_register(SoundDriver *d, SoundBank *bank, const uint8_t *file, size_t size);

#endif
