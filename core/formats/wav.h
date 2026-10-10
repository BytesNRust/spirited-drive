/* Canonical RIFF/WAVE PCM for the sound exchange. The game stores 8-bit samples as signed bytes (checked on
 * the CD's O*MB and COLL_SB banks: the mass sits around 0 and 255, not 128), WAVE stores them unsigned, so
 * both directions convert. 16-bit samples are little-endian in both. */
#ifndef TNFS_WAV_H
#define TNFS_WAV_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    WAV_OK = 0,
    WAV_ERR_MEMORY,
    WAV_ERR_FORMAT,   /* not RIFF/WAVE, or no fmt / data chunk */
    WAV_ERR_UNSUPPORTED, /* not 8- or 16-bit PCM, or not mono / stereo */
    WAV_ERR_TRUNCATED /* data chunk runs past the end of the file */
} WavResult;

typedef struct {
    uint32_t rate;
    int bits;          /* 8 or 16 */
    int channels;      /* 1 or 2 */
    size_t frames;
} WavInfo;

/* `pcm` is `frames * bits/8 * channels` bytes as the game stores them. Output is malloc'd. */
WavResult wav_write(const uint8_t *pcm, const WavInfo *info, uint8_t **out, size_t *out_len);

/* Returns the samples in the game's byte order (malloc'd, free with free()). */
WavResult wav_read(const uint8_t *data, size_t len, WavInfo *info, uint8_t **pcm);

const char *wav_result_str(WavResult result);

#endif
