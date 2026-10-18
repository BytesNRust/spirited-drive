/* The link game in Control Central (link_central.h): the race_net calls the
 * menus make, on the link game's words and the bound menu's settings. Each
 * op that can take a message in (the service, a send, the menu reset) puts
 * the words back to their homes first and reads them again after. */
#include "game/frontend/link_central.h"

#include "game/frontend/frontend.h"
#include "game/frontend/link_sync.h"
#include "game/frontend/menu_glue.h"
#include "game/frontend/net_host.h"
#include "game/sys/resource.h"

static LinkGame *const L = &link_game;
static const LinkMenu *M;

/* 00524e8b, then 00524e8d's eight: 004465e4 reads the word before 00524e8d */
static int16_t cars[1 + RACE_NET_PLAYERS];

void link_central_bind(const LinkMenu *menu) { M = menu; }

static void lock(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static void service(void *ctx) { (void)ctx, link_service(); }
static int16_t german(void) { return (int16_t)resource_german(); }

/* --- 004465e4 and 00411050 --- */

static void seat_close(void *ctx, const RaceNetLink *entry)
{
    (void)ctx;
    link_close_entry(link_of((int)(entry - L->entries)));
}

static void seat_forget(void *ctx, uint32_t session, const uint8_t *name)
{
    (void)ctx, (void)session;
    link_forget(name);
}

static void seats(void *ctx)
{
    (void)ctx;
    link_entries_sync();
    RaceNetSeats s = { menu_words.mode_menu, menu_words.link_game, *M->tournament,
                       (const uint8_t *)frontend_player_name, L->state, L->links, L->entries, L->tags, L->sets + 1,
                       cars + 1 };
    static const RaceNetSeatOps ops = { NULL, seat_close, seat_forget };
    race_net_seat(&ops, &s);
}

static void update(void *ctx) { (void)ctx, link_add_new_players(); }
static void send(void *ctx, uint32_t link, const uint8_t *data, int32_t size) { (void)ctx, link_send(link, data, size); }

static void poll(void)
{
    const RaceNetPoll p = { menu_words.mode_menu, 0, L->state, L->links };
    static const RaceNetPollOps ops = { NULL, update, lock, seats, lock, service, send };
    race_net_poll(&ops, &p);
}

/* --- 004104d0 --- */

static void sync_in(RaceNetSync *s)
{
    cars[0] = *M->player_car;
    for (int i = 0; i < RACE_NET_PLAYERS; i++) cars[1 + i] = M->other_car[i];
    *s = (RaceNetSync){ menu_words.link_game, menu_words.mode_menu, L->unknown_52b302, *M->tournament, L->view,
                        L->unknown_52b318, menu_words.redraw, L->flags_a[7], L->flags_a[8], 0, L->state, L->incoming,
                        L->settings, menu_words.single, *M->race_type, *M->track, *M->laps, *M->difficulty,
                        *M->net_car, *M->nomercy, cars, L->sets + 1, *M->variant_m, *M->variant_r };
}

static void sync_out(const RaceNetSync *s)
{
    menu_words.link_game = s->play, menu_words.mode_menu = s->host, L->unknown_52b302 = s->unknown_52b302;
    *M->tournament = s->unknown_52b310, L->view = s->unknown_52b316, L->unknown_52b318 = s->unknown_52b318;
    menu_words.redraw = s->unknown_52b31a, L->flags_a[7] = s->unknown_50a5ba, L->flags_a[8] = s->unknown_50a5bc;
    menu_words.single = s->followed, *M->race_type = s->race_type, *M->track = s->track;
    *M->laps = s->unknown_524ea1, *M->difficulty = s->unknown_524ea3, *M->net_car = s->unknown_524e9d;
    *M->nomercy = s->unknown_4c5cc2, *M->variant_m = s->unknown_4fbad8, *M->variant_r = s->unknown_4fbadc;
    *M->player_car = cars[0];
    for (int i = 0; i < RACE_NET_PLAYERS; i++) M->other_car[i] = cars[1 + i];
}

static void sync_service(void *ctx) { sync_out(ctx), link_service(), sync_in(ctx); }
static void sync_poll(void *ctx) { sync_out(ctx), poll(), sync_in(ctx); }
static void sync_menu(void *ctx) { sync_out(ctx), glue_game_reset(), sync_in(ctx); }

static void sync_send(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    sync_out(ctx), link_send(link, data, size), sync_in(ctx);
}

void link_sync(void)
{
    if (M == NULL) return;
    RaceNetSync s;
    sync_in(&s);
    const RaceNetSyncOps ops = { &s, sync_service, lock, lock, sync_poll, sync_send, sync_menu };
    race_net_sync(&ops, &s);
    sync_out(&s);
}

/* --- 00410c18 --- */

static void wait_in(RaceNetWait *w)
{
    *w = (RaceNetWait){ menu_words.link_game, menu_words.link_lost, L->unknown_52b302, menu_words.intro_seen, L->state,
                        L->links, L->entries, L->flags_a, L->flags_b };
}

static void wait_out(const RaceNetWait *w)
{
    menu_words.link_game = w->play, menu_words.link_lost = w->dropped, L->unknown_52b302 = w->unknown_52b302;
    menu_words.intro_seen = w->unknown_4fbb0c;
}

static void wait_send(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    wait_out(ctx), link_send(link, data, size), wait_in(ctx);
}

static void wait_service(void *ctx) { wait_out(ctx), link_service(), wait_in(ctx); }
static int32_t wait_step(void *ctx) { wait_out(ctx); const int32_t r = link_wait_step(); wait_in(ctx); return r; }
static void wait_drop(void *ctx, int16_t slot) { wait_out(ctx), link_drop(slot), wait_in(ctx); }
static void wait_sync(void *ctx) { wait_out(ctx), link_sync(), wait_in(ctx); }

int32_t link_wait(int16_t stage)
{
    link_entries_sync();
    RaceNetWait w;
    wait_in(&w);
    const RaceNetWaitOps ops = { &w, wait_send, wait_service, wait_step, wait_drop, wait_sync };
    const int32_t r = race_net_wait(&ops, &w, stage);
    wait_out(&w);
    return r;
}

/* --- 0044633c and 00446284 --- */

static void ready_in(RaceNetReady *r)
{
    *r = (RaceNetReady){ menu_words.redraw, L->flags_a[8], L->flags_a[7], L->flags_a[RACE_NET_STAGE_FLAG], german(),
                         L->unknown_52b302 };
}

static void ready_sync(void *ctx) { link_sync(), ready_in(ctx); }
static void ready_message(void *ctx, uint32_t text, int32_t ticks) { link_message(text, ticks), ready_in(ctx); }
static void ready_deadline(void *ctx, int32_t ticks) { (void)ctx, link_deadline(ticks); }
static int32_t ready_wait(void *ctx, int16_t stage) { const int32_t r = link_wait(stage); ready_in(ctx); return r; }
static void ready_service(void *ctx) { link_service(), ready_in(ctx); }

int32_t link_ready(void)
{
    if (M == NULL) return 0;
    RaceNetReady r;
    ready_in(&r);
    const RaceNetReadyOps ops = { &r, ready_sync, ready_message, ready_deadline, ready_wait, ready_service };
    return race_net_ready(&ops, &r);
}

static void host_in(RaceNetHostReady *r)
{
    *r = (RaceNetHostReady){ menu_words.link_game, menu_words.intro_seen, german(), L->unknown_52b302, menu_words.redraw,
                             menu_words.link_lost };
}

static void host_out(const RaceNetHostReady *r)
{
    menu_words.link_game = r->play, menu_words.intro_seen = r->unknown_4fbb0c, L->unknown_52b302 = r->unknown_52b302;
    menu_words.redraw = r->unknown_52b31a, menu_words.link_lost = r->dropped;
}

static void host_message(void *ctx, uint32_t text, int32_t ticks) { host_out(ctx), link_message(text, ticks), host_in(ctx); }
static void host_close(void *ctx) { host_out(ctx), link_close_session(), host_in(ctx); }
static void host_sync(void *ctx) { host_out(ctx), link_sync(), host_in(ctx); }
static void host_service(void *ctx) { host_out(ctx), link_service(), host_in(ctx); }
static int32_t host_wait(void *ctx, int16_t stage) { host_out(ctx); const int32_t r = link_wait(stage); host_in(ctx); return r; }

void link_host_ready(void)
{
    if (M == NULL) return;
    RaceNetHostReady r;
    host_in(&r);
    const RaceNetHostReadyOps ops = { &r, host_message, host_close, host_sync, host_service, ready_deadline, host_wait };
    race_net_host_ready(&ops, &r);
    host_out(&r);
}

/* --- 004109b4, 00432e90 and the words Central reads --- */

int16_t link_any_ready(void) { return race_net_any_ready(menu_words.link_game, L->settings); }

static void kick_drop(void *ctx, int16_t slot) { (void)ctx, link_drop(slot); }

void link_kick(int32_t player)
{
    static const NetKickOps ops = { glue_yes_no, kick_drop, NULL };
    net_kick(german(), player, &ops);
}

int16_t link_view_only(void) { return L->view; }
int16_t link_humans(void) { return link_word(L->state + RACE_NET_AT_HUMANS); }
const char *link_names(void) { return (const char *)L->state + RACE_NET_AT_NAMES; }
