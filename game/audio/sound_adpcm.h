/* The sound driver's IMA ADPCM decoders (the compressed samples, flag 2:
 * the music's streams). The mixer's compressed spans (00498198, 004982a5,
 * 004983c5) call them through the voice's +0x58 into 004eb8ac. */
#ifndef TNFS_SOUND_ADPCM_H
#define TNFS_SOUND_ADPCM_H

#include <stdint.h>

/* The step index is kept times 64 (a row of the delta table 004f48a0, 16
 * words), 0..0x1600 (88 steps). */
enum { SOUND_ADPCM_INDEX_MAX = 0x1600 };

/* 004a553c (cdecl first, src, dst, end, &sample, &index): mono, nibble by
 * nibble from nibble `first` of `src` (an even one is a byte's high half),
 * until nibble `end` - not counting `first` (end += first); one at least
 * (the loop tests after a sample). *sample's low 16 bits signed are the
 * predictor, *index the step index x 64; both written back as 32-bit
 * words. Each sample saturated to s16, the index held to 0..0x1600. */
void sound_adpcm_mono(uint32_t first, const uint8_t *src, int16_t *dst, int32_t count, int32_t *sample, uint32_t *index);

/* 004a55e4 (the same arguments; `first` unused): stereo, a byte a frame,
 * its high nibble left and low nibble right, into interleaved frames;
 * *sample and *index hold left in the low half and right in the high half
 * (the predictors as s16). One frame at least. */
void sound_adpcm_stereo(const uint8_t *src, int16_t *dst, int32_t count, uint32_t *sample, uint32_t *index);

#endif
