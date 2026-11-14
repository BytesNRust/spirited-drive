/* A car's palettes in its CFM's bank (car_model.h): a mod's car (fuzz/car_model.c) whose '!PAL' pointed outside its bank made the load
 * write 768 colours there; a palette is taken only when its colours and the area the tint reads are inside the bank */
#include "tests/test.h"

#include "game/race/car_model.h"

#include <string.h>

enum { HEADER = 16, DIR = 8, ENTRY = 16, PALETTE = 768 };

static uint8_t bank[HEADER + 2 * DIR + 2 * (ENTRY + PALETTE)];

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

/* '!PAL' and '!xxx', 256 x 3 palettes, at their offsets */
static size_t build(uint32_t pal_at, uint32_t own_at)
{
    memset(bank, 0, sizeof bank);
    memcpy(bank, "SHPI", 4), put32(bank + 4, sizeof bank), put32(bank + 8, 2), memcpy(bank + 12, "LN32", 4);
    memcpy(bank + HEADER, "!PAL", 4), put32(bank + HEADER + 4, pal_at);
    memcpy(bank + HEADER + DIR, "!xxx", 4), put32(bank + HEADER + DIR + 4, own_at);
    for (uint32_t at = HEADER + 2 * DIR; at + ENTRY <= sizeof bank; at += ENTRY + PALETTE)
        bank[at] = 0x24, put16(bank + at + 4, 256), put16(bank + at + 6, 3);
    return sizeof bank;
}

static const uint8_t PAL[4] = { '!', 'P', 'A', 'L' }, OWN[4] = { '!', 'x', 'x', 'x' };

int main(void)
{
    const uint32_t first = HEADER + 2 * DIR, second = first + ENTRY + PALETTE;
    Fsh fsh;
    CHECK_EQ_INT(fsh_open(&fsh, bank, build(first, second)), FSH_OK);
    CHECK(car_model_bank_shape(&fsh, PAL) == bank + first);   /* in the bank: as before */
    CHECK(car_model_bank_shape(&fsh, OWN) == bank + second);

    CHECK_EQ_INT(fsh_open(&fsh, bank, build(first, 0x7ffffff0)), FSH_OK);   /* far past the bank */
    CHECK(car_model_bank_shape(&fsh, OWN) == NULL);
    const uint32_t late = second + ENTRY + 100;   /* a 0x24 palette whose 768 colours run past the end */
    CHECK_EQ_INT(fsh_open(&fsh, bank, build(first, late)), FSH_OK);
    bank[late] = 0x24, put16(bank + late + 4, 16), put16(bank + late + 6, 3);
    CHECK(car_model_bank_shape(&fsh, OWN) == NULL);
    CHECK_EQ_INT(fsh_open(&fsh, bank, build(first, second)), FSH_OK);
    put16(bank + second + 6, 40);   /* 256 x 40: the tint would read 10240 bytes of 768 */
    CHECK(car_model_bank_shape(&fsh, OWN) == NULL);
    CHECK(car_model_bank_shape(&fsh, PAL) == bank + first);

    /* the CD's own colours: a 256 x 1 bitmap of 256 bytes that ends the bank (WARRIOR.CFM's) */
    const size_t own_end = second + ENTRY + 256;
    build(first, second);
    bank[second] = 0x7B, put16(bank + second + 4, 256), put16(bank + second + 6, 1);
    CHECK_EQ_INT(fsh_open(&fsh, bank, own_end), FSH_OK);
    CHECK(car_model_bank_shape(&fsh, OWN) == bank + second);
    CHECK_EQ_INT(fsh_open(&fsh, bank, second + ENTRY + 60), FSH_OK);   /* fewer than the 0x17 colours the race reads */
    put16(bank + second + 4, 60);
    CHECK(car_model_bank_shape(&fsh, OWN) == NULL);
    return TEST_REPORT();
}
