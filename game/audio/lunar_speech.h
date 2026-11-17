/* port (dep59, Lunar Springs' own entry, notes/dep59-lunar-entry-plan.md L3): the track detail's speech for Lunar Springs, the CD's two
 * unused lines played as one, as the owner chose (2026-11-16): LUNARWEL.EAS ("Welcome to Lunar Springs!"), a pause of
 * LUNAR_SPEECH_GAP_MS and LUNARGOL.EAS ("While you are up here, see if you can find the golf pot!"). Joined in memory into one EACS
 * sample (the speech queue's kind: PCM, no compression); nothing is written to disk. No files here: the caller reads them. */
#ifndef TNFS_LUNAR_SPEECH_H
#define TNFS_LUNAR_SPEECH_H

#include <stddef.h>
#include <stdint.h>

enum { LUNAR_SPEECH_GAP_MS = 350, EACS_HEADER_SIZE = 0x20 };

/* the name the menus ask for (track_select's speech table) and the two files it is made of, in the speech folder (speech/ or gspeech/) */
extern const char LUNAR_SPEECH_NAME[], LUNAR_SPEECH_WELCOME[], LUNAR_SPEECH_GOLF[];

/* `a`, a pause of `gap_ms` and `b` as one EACS sample in `out` (`cap` bytes): `a`'s header with the frames of the three, its data at
 * EACS_HEADER_SIZE. The bytes it takes (written when `out` is not NULL and they fit); 0 when either is no plain EACS sample (magic,
 * no compression, its data at 0x20 and inside the file) or the two differ in rate, sample size or channels. */
size_t lunar_speech_join(const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size, uint32_t gap_ms, uint8_t *out, size_t cap);

#endif
