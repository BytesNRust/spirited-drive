/* WAV: the studio's and the sound exchange's reader */
#include "fuzz/fuzz.h"
#include "formats/wav.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    WavInfo info;
    uint8_t *pcm = NULL;
    if (wav_read(data, size, &info, &pcm) == WAV_OK) fuzz_touch(pcm, info.frames * (size_t)(info.bits / 8) * (size_t)info.channels);
    free(pcm);
    return 0;
}
