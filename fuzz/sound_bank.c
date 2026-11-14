/* A sound bank (.bnk, the CD's or a mod's) as the race registers it, every sound played through the mixer and then taken back */
#include "fuzz/fuzz.h"
#include "game/audio/sound_bank.h"
#include "game/audio/sound_driver.h"

#include <stdlib.h>
#include <string.h>

enum { RATE = 0x5622, FRAMES = 512 };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static SoundDriver d;
    static SoundBank bank;
    memset(&d, 0, sizeof d), memset(&bank, 0, sizeof bank);
    sound_driver_wire(&d);
    sound_channels_reset(&d.channels);
    d.channels.open = 1;
    sound_driver_init(&d, RATE);
    uint8_t *file = calloc(1, size + SOUND_BANK_PAD);   /* the zero bytes the race's banks have after them (sound_bank.h) */
    if (file == NULL) return 0;
    if (size) memcpy(file, data, size);
    const int32_t slot = sound_bank_register(&d, &bank, file, size);
    if (slot < 0) return free(file), 0;
    static int16_t out[2 * FRAMES];
    for (int id = 0; id < SOUND_BANK_SOUNDS; id++) {
        if (!bank.has[id]) continue;
        (void)sound_play(&d, id, 0x40, 0x7f, 0x40);
        (void)sound_mix_frames(&d.mixer, out, FRAMES);
    }
    for (int i = 0; i < 8; i++) (void)sound_mix_frames(&d.mixer, out, FRAMES);
    (void)sound_bank_unregister(&d, slot);
    free(file);
    return 0;
}
