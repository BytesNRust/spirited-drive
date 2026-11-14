/* The weather's WAV samples (data/sounds, which a mod can replace): thunder, the wet roll, the splashes */
#include "fuzz/fuzz.h"
#include "game/audio/splash.h"
#include "game/audio/thunder.h"
#include "game/audio/wet_roll.h"

#include <string.h>

enum { RATE = 0x5622 };   /* the mixer's (drive_audio.c) */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static Thunder t;
    static WetRoll w;
    static Splash s;
    memset(&t, 0, sizeof t), memset(&w, 0, sizeof w), memset(&s, 0, sizeof s);
    if (thunder_load(&t, data, size, RATE)) thunder_free(&t);
    if (wet_roll_load(&w, data, size, RATE)) wet_roll_free(&w);
    if (splash_load(&s, data, size, RATE)) splash_free(&s);
    return 0;
}
