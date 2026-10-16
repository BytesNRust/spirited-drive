/* 0042702c: the frontend's settings into the setup and the two records */
#include "race_from_menu.h"

#include "race_models.h"
#include "race_session.h"

#include <string.h>

enum {
    NAME = 2, MODEL = 0xb, CAR_0F = 0xf, FLAG_13 = 0x13, FLAG_17 = 0x17, W1B = 0x1b, W1F = 0x1f, W23 = 0x23,
    W27 = 0x27, W2B = 0x2b, W2F = 0x2f, W3F = 0x3f, W43 = 0x43
};

static int32_t le32(const uint8_t *b) { return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24); }
static void put32(uint8_t *b, int32_t v) { for (int i = 0; i < 4; i++) b[i] = (uint8_t)((uint32_t)v >> (8 * i)); }

/* config.dat's word at `at` (00524e5d..00524ea3) */
static int32_t word(const RaceFromMenu *m, uint32_t at)
{
    const uint8_t *b = m->frontend + (at - RACE_FROM_MENU_AT);
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}
static void set_word(RaceFromMenu *m, uint32_t at, int16_t v)
{
    uint8_t *b = m->frontend + (at - RACE_FROM_MENU_AT);
    b[0] = (uint8_t)v, b[1] = (uint8_t)((uint16_t)v >> 8);
}
static void set(RaceFromMenu *m, uint32_t at, int32_t v) { put32(m->setup + (at - 0x4fb7bc), v); }
static int32_t get(const RaceFromMenu *m, uint32_t at) { return le32(m->setup + (at - 0x4fb7bc)); }

static void players(RaceFromMenu *m)
{
    if (m->play != 0) {
        m->unknown_4fbb0c = word(m, 0x524e7b);
        m->followed = m->viewed = m->net_player;
        set(m, 0x4fb7d4, 1), set(m, 0x4fb7e0, m->net_humans);
        return;
    }
    m->unknown_4fbb0c = 1;
    set(m, 0x4fb7d4, 0), set(m, 0x4fb7e0, 1);
    m->followed = m->viewed = 0;
}

/* +0x20 racers, +0x28 police, +0x30 traffic */
static void cars(RaceFromMenu *m)
{
    const int32_t type = word(m, 0x524e5d);
    int32_t racers = 2, police = 0, traffic = 0;
    if (type == 3 || type == 2) racers = 8;
    else if (type == 0) racers = 1;
    else if ((m->setup[0x1c] & 4) == 0 && get(m, 0x4fb7c2) <= 2) police = m->play == 0, traffic = 6;
    set(m, 0x4fb7dc, racers), set(m, 0x4fb7e4, police), set(m, 0x4fb7ec, traffic);
}

static void memory_cuts(RaceFromMenu *m)
{
    if (m->memory_4baec8 < 0x57800) set_word(m, 0x524e6d, 0);
    if (m->memory_4baecc < 0x47b760 && word(m, 0x524e6f) == 0) {
        set_word(m, 0x524e73, 0), set_word(m, 0x524e6d, 0), set_word(m, 0x524e6f, 1);
        set_word(m, 0x524e61, 1), set_word(m, 0x524e77, 1);
        m->unknown_4b95ac = 1;
    } else if (m->memory_4baecc < 0x4b3a90 && word(m, 0x524e6f) == 0) {
        set_word(m, 0x524e6f, 1), set_word(m, 0x524e73, 0);
    } else if (word(m, 0x524e61) == 0 && m->memory_4baecc < 0x50df20 && word(m, 0x524e6f) == 0) {
        set_word(m, 0x524e61, 1);
    }
}

static void first_record(RaceFromMenu *m, uint8_t *r)
{
    const int on = m->sound != 0;
    put32(r + W2F, on ? word(m, 0x524e6d) : 0), put32(r + W1F, on ? word(m, 0x524e69) : 0);
    put32(r + W23, on ? word(m, 0x524e65) : 0), put32(r + W2B, on ? m->sound : 0);
    put32(r + W1B, m->unknown_52b30a), put32(r + W27, word(m, 0x524e61));
    for (int k = 0; NAME + k < RACE_SESSION_RECORD; k++) {  /* port guard: within the record */
        r[NAME + k] = m->frontend[k];
        if (m->frontend[k] == 0) break;
    }
    put32(r + MODEL, (int16_t)race_head_to_head_model((int16_t)word(m, 0x524e8b)));
    put32(r + CAR_0F, word(m, 0x524e7d));
    const int32_t f13 = word(m, 0x524e7f) == 0;
    put32(r + FLAG_13, f13), m->unknown_4bafec = f13;
    const int32_t f17 = word(m, 0x524e89) == 0;
    put32(r + FLAG_17, f17), m->unknown_4bafe8 = f17;
}

void race_from_menu(RaceFromMenu *m, uint8_t *first, uint8_t *second)
{
    set(m, 0x4fb7be, word(m, 0x524e83)), set(m, 0x4fb7c2, word(m, 0x524e85));
    set(m, 0x4fb7d0, word(m, 0x524e5d)), set(m, 0x4fb7f0, word(m, 0x524e87));
    set(m, 0x4fbac4, word(m, 0x524ea1)), set(m, 0x4fbabc, 1), set(m, 0x4fbac0, 4);
    set(m, 0x4fbad4, word(m, 0x524ea3)), set(m, 0x4fb7d8, m->unknown_52b306);
    players(m);
    cars(m);
    memory_cuts(m);
    first_record(m, first);
    memcpy(second, first, RACE_SESSION_RECORD);
    put32(second + W3F, 0), put32(second + W43, 0);
    put32(second + MODEL, (int16_t)race_head_to_head_model((int16_t)word(m, 0x524e81)));
    m->unknown_4fa040 = m->unknown_4ae1d4;
    set(m, 0x4fbac0, (m->setup[0x1c] & 4) == 0);
    set(m, 0x4fbad0, word(m, 0x524e9d));
}
