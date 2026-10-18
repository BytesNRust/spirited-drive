/* A link game's race (link_race.h): the drive's link from the link game's
 * words, its sends, waits and slots through the race_net glue. */
#include "game/frontend/link_race.h"

#include "game/frontend/link_central.h"
#include "game/frontend/link_game.h"
#include "game/frontend/link_sync.h"
#include "game/frontend/menu_glue.h"
#include "game/race/race_models.h"
#include "game/viewer/track_viewer.h"

static LinkGame *const L = &link_game;

/* 00445e00: the words little-endian, as the original's memory holds them */
static void race_send(int32_t to, const int16_t *words, int32_t size)
{
    uint8_t data[RACE_NET_STATE];
    if (size < 0 || size > (int32_t)sizeof data) return;
    for (int32_t i = 0; i + 1 < size; i += 2)
        data[i] = (uint8_t)words[i / 2], data[i + 1] = (uint8_t)((uint16_t)words[i / 2] >> 8);
    if (size & 1) data[size - 1] = (uint8_t)words[size / 2];
    link_send((uint32_t)to, data, size);
}

static void (*chat_screen)(void *ctx);
static void chat(void) { chat_screen(NULL); }

/* 00410c18 on the link game's words */
static int32_t race_wait(int32_t stage) { return link_wait((int16_t)stage); }

void link_race_begin(int16_t own_car, const int16_t *cars_by_slot, void (*chat_with)(void *ctx))
{
    chat_screen = chat_with;
    TrackViewerLink l = { link_humans(), menu_words.single, menu_words.intro_seen, { 0 }, race_send, link_service,
                          race_wait, link_slot_of, link_sync, link_host_ready, link_deadline, link_any_ready, chat,
                          &L->flags_a[RACE_NET_STAGE_FLAG], &menu_words.link_lost };
    if (l.humans > TRACK_VIEWER_RACERS) l.humans = TRACK_VIEWER_RACERS;
    for (int32_t i = 0; i < l.humans; i++)
        l.cars[i] = (int)race_head_to_head_model(i == l.slot ? own_car : cars_by_slot[i]);
    track_viewer_drive_link(&l);
    L->race_message = track_viewer_drive_message;
}

void link_race_end(void) { L->race_message = NULL; }
