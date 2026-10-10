/* BNK: a sound bank (the .BNK files in SIMDATA/SOUNDBNK). 512 bytes of 128 u32 offsets
 * (0 = no sample in the slot), each pointing at a 72-byte header; raw PCM
 * follows at the offset the header gives. The parser keeps a pointer into the
 * caller's buffer and allocates nothing. Layout checked on all 38 CD files;
 * the loader in the EXE is not yet traced, so flags and the unnamed header
 * bytes are kept verbatim in BnkSample.header.
 *
 *   +0x00 u32  flags (meaning differs by bank family)
 *   +0x04 u32  header offset + 0x28
 *   +0x28      "EACS"
 *   +0x2c u32  rate: 16000 in 299 of 334 CD samples, 16247 in 27 and 22050 in 8
 *                (collision banks)
 *   +0x30 u8   1 = 8-bit, 2 = 16-bit
 *   +0x31 u8   1 = mono, 2 = stereo
 *   +0x34 u32  loop start, +0x38 loop length, +0x3c length (all in frames)
 *   +0x40 u32  offset of the PCM in the file
 * The length is 0 in the collision and gear-change samples: their data runs
 * to the next sample's data (bnk_data_extent). */
#ifndef TNFS_BNK_H
#define TNFS_BNK_H

#include <stddef.h>
#include <stdint.h>

enum {
    BNK_SLOTS = 128,
    BNK_TABLE_SIZE = BNK_SLOTS * 4,
    BNK_HEADER_SIZE = 72
};

typedef enum {
    BNK_OK = 0,
    BNK_ABSENT,         /* slot holds offset 0 */
    BNK_ERR_TRUNCATED,  /* file shorter than the offset table */
    BNK_ERR_SLOT,       /* slot outside 0..127 */
    BNK_ERR_BOUNDS,     /* header outside the file */
    BNK_ERR_HEADER,     /* no EACS at +0x28, +4 inconsistent, or unknown bit depth / channels */
    BNK_ERR_DATA        /* declared PCM runs past the end of the file */
} BnkResult;

typedef struct {
    const uint8_t *data;
    size_t size;
} Bnk;

typedef struct {
    uint32_t header_offset;
    uint32_t flags;
    uint32_t rate;
    int bits;          /* 8 or 16 */
    int channels;      /* 1 or 2 */
    uint32_t loop_start, loop_length, length;  /* frames */
    uint32_t data_offset;
    uint8_t header[BNK_HEADER_SIZE];  /* the whole header, for writers */
} BnkSample;

BnkResult bnk_open(Bnk *bnk, const uint8_t *data, size_t size);
BnkResult bnk_sample(const Bnk *bnk, int slot, BnkSample *out);

/* Bytes per frame: bits / 8 x channels. */
int bnk_frame_bytes(const BnkSample *sample);

/* Bytes from the sample's data up to the next higher data offset of any
 * sample in the bank, or to the end of the file. */
size_t bnk_data_extent(const Bnk *bnk, const BnkSample *sample);

const char *bnk_result_str(BnkResult result);

#endif
