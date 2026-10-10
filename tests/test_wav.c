/* WAV tests: write and read agree, 8-bit data is signed in the game and unsigned in the file, odd chunks
 * are padded, foreign chunks skipped, and unsupported or damaged files are refused. */
#include "formats/wav.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void test_round_trip(void)
{
    const uint8_t pcm16[8] = { 1, 2, 0xff, 0x7f, 0, 0x80, 5, 6 };    /* 2 frames stereo 16-bit */
    WavInfo in = { 16000, 16, 2, 2 }, out;
    uint8_t *file = NULL, *back = NULL;
    size_t len = 0;
    CHECK_EQ_INT(wav_write(pcm16, &in, &file, &len), WAV_OK);
    CHECK_EQ_INT(len, 44 + 8);
    CHECK(memcmp(file, "RIFF", 4) == 0 && memcmp(file + 36, "data", 4) == 0);
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_OK);
    CHECK_EQ_INT(out.rate, 16000);
    CHECK_EQ_INT(out.bits, 16);
    CHECK_EQ_INT(out.channels, 2);
    CHECK_EQ_INT(out.frames, 2);
    CHECK_MEM(back, pcm16, 8);
    free(file);
    free(back);

    const uint8_t pcm8[3] = { 0x00, 0x7f, 0x80 };                     /* signed: 0, +127, -128 */
    WavInfo in8 = { 22050, 8, 1, 3 };
    CHECK_EQ_INT(wav_write(pcm8, &in8, &file, &len), WAV_OK);
    CHECK_EQ_INT(len, 44 + 3 + 1);                                    /* data chunk padded to even */
    CHECK_EQ_INT(file[44], 0x80);                                     /* 0 -> 128 unsigned */
    CHECK_EQ_INT(file[45], 0xff);
    CHECK_EQ_INT(file[46], 0x00);
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_OK);
    CHECK_EQ_INT(out.frames, 3);
    CHECK_MEM(back, pcm8, 3);
    free(back);

    /* a LIST chunk before data is skipped */
    uint8_t extra[64];
    memcpy(extra, file, 36);
    memcpy(extra + 36, "LIST", 4);
    extra[40] = 4; extra[41] = extra[42] = extra[43] = 0;
    memcpy(extra + 44, "abcd", 4);
    memcpy(extra + 48, file + 36, 8 + 4);
    CHECK_EQ_INT(wav_read(extra, 48 + 12, &out, &back), WAV_OK);
    CHECK_EQ_INT(out.frames, 3);
    free(back);
    free(file);
}

static void test_refusals(void)
{
    uint8_t *file = NULL, *back = NULL;
    size_t len = 0;
    WavInfo info = { 16000, 16, 1, 4 }, out;
    const uint8_t pcm[8] = {0};
    CHECK_EQ_INT(wav_write(pcm, &info, &file, &len), WAV_OK);
    CHECK_EQ_INT(wav_read(file, 10, &out, &back), WAV_ERR_FORMAT);
    CHECK_EQ_INT(wav_read(file, len - 2, &out, &back), WAV_ERR_TRUNCATED);
    file[34] = 24;                                                    /* 24-bit */
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_ERR_UNSUPPORTED);
    file[34] = 16; file[20] = 3;                                      /* float */
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_ERR_UNSUPPORTED);
    file[20] = 1; file[22] = 6;                                       /* 6 channels */
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_ERR_UNSUPPORTED);
    memcpy(file, "RIFX", 4);
    CHECK_EQ_INT(wav_read(file, len, &out, &back), WAV_ERR_FORMAT);
    free(file);
    WavInfo bad = { 16000, 12, 1, 4 };
    CHECK_EQ_INT(wav_write(pcm, &bad, &file, &len), WAV_ERR_UNSUPPORTED);
}

int main(void)
{
    test_round_trip();
    test_refusals();
    return TEST_REPORT();
}
