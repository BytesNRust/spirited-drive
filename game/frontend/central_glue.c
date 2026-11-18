#include "game/frontend/central_glue_private.h"
#include "game/frontend/card_marks.h"
#include "game/frontend/car_pick.h"
#include "game/frontend/track_pick.h"
#include "formats/stock_content.h"
#include "game/race/weather.h"
#include "game/frontend/lunar_art.h"
#include "game/frontend/added_best_art.h"
#include "game/sys/car_pieces.h"
#include "game/sys/edition_file.h"
#include "game/sys/car_looks.h"
#include "game/sys/text.h"
#include "game/sys/track_pieces.h"
#include "game/sys/track_registry.h"
#include "game/frontend/edition_rules.h"
#include "game/frontend/unlock_level.h"
#include "game/frontend/variant_rule.h"
#include "game/frontend/tournament_variants.h"

CentralGlobals cg_g = { .was = { CENTRAL_NO_ART, CENTRAL_NO_ART, CENTRAL_NO_ART, CENTRAL_NO_ART, CENTRAL_NO_ART, CENTRAL_NO_ART } };  /* 004bafb4..'s data: no art shown yet */

enum { MOVIES_AT = 0x5b8 };                        /* 0052b2fc: 2, the movies offered (the original's default) */
uint8_t cg_game_state[GAME_STATE_SIZE] = { [MOVIES_AT] = 2 };  /* 0052b2c0..: the tournament's */

/* 0052b2fc: the movies the game offers (the showcase's video page, the arrest's and the wins' movies); the port's
 * no_movies only skips the intro (intro_glue.c) */
int16_t glue_movies(void)
{
    return (int16_t)(cg_game_state[MOVIES_AT] | cg_game_state[MOVIES_AT + 1] << 8);
}
static uint8_t keys[0x100];                       /* 004c693c: by scancode, refreshed at each key (00464bd0) */
uint8_t cg_screen_palette[PALETTE_PUT_BYTES];

/* port (dep59): the player's car and the rival as picked (an added car among them) and as config.dat's image holds them (car_pick.h) */
static CarPick pick_player, pick_opponent;

RaceSettings cg_settings_of(void)
{
    RaceSettings s = { cg_g.race_type, cg_g.track, cg_g.variant_m, cg_g.variant_r, car_pick_to_image(&pick_player, cg_g.player_car),
                       car_pick_to_image(&pick_opponent, cg_g.opponent), cg_g.laps, cg_g.difficulty, cg_g.car_flag_a, cg_g.car_flag_b,
                       cg_g.variant, { 0 }, cg_g.net_car };
    for (int i = 0; i < 8; i++) s.other_car[i] = cg_g.other_car[i];
    return s;
}

/* port (dep59): an added pick comes back only where one is offered: the player's car outside a tournament (whose class sets the car), the
 * rival in a head to head */
void cg_settings_put(const RaceSettings *s)
{
    const int16_t player = (int16_t)s->player_car, opponent = (int16_t)s->opponent;
    cg_g.race_type = (int16_t)s->race_type, cg_g.track = (int16_t)s->track;
    cg_g.player_car = s->race_type == RACE_TOURNAMENT ? player : car_pick_from_image(&pick_player, player);
    cg_g.variant_m = s->variant_m, cg_g.variant_r = s->variant_r;
    cg_g.opponent = s->race_type == RACE_HEAD_TO_HEAD ? car_pick_from_image(&pick_opponent, opponent) : opponent;
    cg_g.laps = (int16_t)s->laps, cg_g.difficulty = (int16_t)s->difficulty, cg_g.variant = (int16_t)s->variant;
    cg_g.car_flag_a = (int16_t)s->car_flag_a, cg_g.car_flag_b = (int16_t)s->car_flag_b, cg_g.net_car = (int16_t)s->unknown_524e9d;
    for (int i = 0; i < 8; i++) cg_g.other_car[i] = (int16_t)s->other_car[i];
}

/* The short 0052ad40 is config.dat's (00524df0 + 0x5f50): the level a cheat raised (central_keys.c) back into the image
 * before a path that reads it from there, and before the image is saved (0042a504) */
void cg_level_to_image(void) { unlock_level_put(config_current(), cg_g.unlock_level); }   /* Restomod: its own (unlock_level.h) */

/* 0052b2d6..0052b2e8, the tournament's races by track that 00432ff4 sums: the menus' game state's (+0x592) */
void cg_races_from_state(void) { memcpy(cg_g.races, cg_game_state + 0x592, sizeof cg_g.races); }

/* The short 0052b302: set while 00427ae0 runs a stage (menu_race.c), the link game's in a link game; the option
 * screens read it for their art and, while clear, keep a link game in step (004104d0) */
static int16_t solo_52b302;
int16_t *cg_word_52b302(void) { return menu_words.link_game != 0 ? &link_game.unknown_52b302 : &solo_52b302; }

Unlocks cg_unlocks_of(void) { return (Unlocks){ cg_g.unlock_m, cg_g.unlock_r, cg_g.lost_vegas, cg_g.hidden, cg_g.warrior }; }

void cg_unlocks_put(const Unlocks *u)
{
    cg_g.unlock_m = u->variant_m, cg_g.unlock_r = u->variant_r, cg_g.lost_vegas = u->lost_vegas, cg_g.hidden = u->hidden;
    cg_g.warrior = (int16_t)u->warrior;
}

static Edition central_edition;   /* port (dep59 step 4): ControlCentralState.edition, read again with the switches (cg_extras_apply) */

/* port (dep59 step 4): 1 when the edition in use closes the tournament (edition_rules.h) */
static int tournament_closed(void) { return central_edition.cars > 0 && !edition_gate(&central_edition).tournament; }

static FrontendSession session_of(void)
{
    FrontendSession f = { cg_g.unlock_level, cg_g.vegas_cheat, menu_words.link_game, cg_g.net_self, cg_g.net_player,
                          menu_words.intro_seen, cg_g.nomercy, { 0 }, cg_g.tournament, resource_german(), 0, tournament_closed() };
    for (int i = 0; i < 6; i++) f.unknown_4bafb4[i] = cg_g.was[i];
    return f;
}

static void session_put(const FrontendSession *f)
{
    cg_g.vegas_cheat = f->unknown_4baf1c, cg_g.net_self = (int16_t)f->unknown_4faa56, cg_g.net_player = (int16_t)f->other_index;
    cg_g.nomercy = (int16_t)f->unknown_4c5cc2, cg_g.tournament = (int16_t)f->tournament_on;
    for (int i = 0; i < 6; i++) cg_g.was[i] = (int16_t)f->unknown_4bafb4[i];
}

/* port: the extras the player left on, over the flags the original owns (central.h names each address) */
static void extras_flags(ExtrasFlags *f)
{
    f->unlock_m = &cg_g.unlock_m;
    f->unlock_r = &cg_g.unlock_r;
    f->variant_m = &cg_g.variant_m;
    f->variant_r = &cg_g.variant_r;
    f->explode = &cg_g.explode;
    f->nitro = &cg_g.nitro;
    f->vegas_cheat = &cg_g.vegas_cheat;
    f->lost_vegas = &cg_g.lost_vegas;
    f->warrior = &cg_g.warrior;
    f->track_flags = (uint32_t *)&menu_cheats.keys_506;
}

/* port: the Warrior-as-rival switch is a separate flag (no equivalent in the original's globals), read by the
 * opponent arrows through ControlCentralState.warrior_rival. Kept up to date with the mode's file by extras_apply. */
static ExtrasSession extras_session;
static int16_t warrior_rival_flag, warrior_keep_flag;   /* and the Warrior kept as the player's car */
static int16_t restomod_flag;   /* port (dep59): Restomod, where restomod.cfg keeps an added car picked */

/* port (dep59): an added car picked comes back from restomod.cfg as Control Central opens (car_looks.h): the player's car, and the head
 * to head rival; one whose mod is gone gives car 0. No line: config.dat's car, as before. Restomod only, never in a link game. */
static const TrackList *central_tracks(TrackList *l);
static void picks_restore(void)
{
    if (restomod_flag == 0) return;
    if (menu_words.link_game != 0) {   /* port (dep59 Lunar L8): a guest keeps no pick from before; the host's rules carry its own */
        cg_lunar_settle();
        return;
    }
    const int car = car_looks_saved_car(0);
    if (car >= 0) cg_g.player_car = (int16_t)car;
    else if (car == CAR_LOOKS_NOT_LOADED) cg_g.player_car = 0;
    /* port (dep59 Lunar L2): track=lunar over config.dat's Rusty rally gives Lunar back where it is offered */
    track_pick_lunar_put(car_looks_saved_lunar());
    cg_lunar_settle();
    /* port (dep59 step 5, T7c): track=<id> of an added track gives it back where the track list offers it (an edition's tracks=); a mod
     * gone, or an edition without it, leaves config.dat's track */
    const int track = car_looks_saved_track();
    TrackList l;
    const TrackList *const offered = track >= TRACK_ADDED_FIRST ? central_tracks(&l) : NULL;
    if (offered != NULL && cg_g.race_type != RACE_TOURNAMENT && track_list_has(offered, (int16_t)track)) cg_g.track = (int16_t)track;   /* a tournament never takes one */
    if (cg_g.race_type != RACE_HEAD_TO_HEAD) return;
    const int rival = car_looks_saved_car(1);
    if (rival >= 0) cg_g.opponent = (int16_t)rival;
    else if (rival == CAR_LOOKS_NOT_LOADED) cg_g.opponent = 0;
}

/* port (dep59): the picks kept in restomod.cfg when they changed: an added car by its id, an original's line taken out (config.dat
 * has it). As Control Central closes and as a race starts. */
void cg_picks_save(void)
{
    if (restomod_flag == 0 || menu_words.link_game != 0) return;
    const int car = car_looks_saved_car(0);
    if (car != cg_g.player_car && (car >= 0 || car_registry_is_added(cg_g.player_car) || car == CAR_LOOKS_NOT_LOADED) &&
        !car_looks_save_car(0, cg_g.player_car))
        port_log("car looks: the car picked was not kept\n");
    if (car_looks_saved_lunar() != track_pick_lunar() && !car_looks_save_lunar(track_pick_lunar()))
        port_log("car looks: the Lunar pick was not kept\n");
    /* port (dep59 step 5, T7c): an added track by its id; an original's (or none) takes the line out, Lunar's staying */
    const int saved = car_looks_saved_track();
    TrackList offered;   /* a line the list does not offer now (no edition, a mod switched off) is not the player's to lose: it stays */
    const TrackList *const list = saved >= TRACK_ADDED_FIRST ? central_tracks(&offered) : NULL;
    const int moved_off = saved >= TRACK_ADDED_FIRST && list != NULL && track_list_has(list, (int16_t)saved);
    if (track_registry_is_added(cg_g.track) ? saved != cg_g.track : moved_off)
        if (!car_looks_save_track(cg_g.track)) port_log("car looks: the track picked was not kept\n");
    if (cg_g.race_type != RACE_HEAD_TO_HEAD) return;
    const int rival = car_looks_saved_car(1);
    if (rival != cg_g.opponent && (rival >= 0 || car_registry_is_added(cg_g.opponent) || rival == CAR_LOOKS_NOT_LOADED) &&
        !car_looks_save_car(1, cg_g.opponent))
        port_log("car looks: the rival picked was not kept\n");
}

/* port: called after every place the original clears the flags, so a switch stays on across races and the
 * tournament. With the mode's file absent this reads the defaults and changes nothing. */
void cg_extras_apply(void)
{
    ExtrasFlags f;
    extras_flags(&f);
    /* port (dep59 step 4): an edition of the player's own settles to what it lists instead (edition_rules.h); the standard edition (or one
     * that cannot be used) leaves central_edition with no cars */
    track_pieces_follow();   /* port (dep59 step 5, T7c): the added tracks of the mods in effect, before the edition that lists them is judged and for the arrows (track_list.h) */
    const int own = edition_active(&central_edition) == 1;
    static Edition session_edition;   /* the edition the session's typed cheats belong to; zero, the standard one */
    if (memcmp(&central_edition, &session_edition, sizeof central_edition) != 0) {   /* another edition: a new session */
        extras_session_restart(&f, &extras_session);
        session_edition = central_edition;
    }
    const PortExtras saved = port_extras_load(), e = own ? edition_extras(&central_edition, saved) : saved;
    /* port: the level, the switches and the cheats typed in the session decide what is on: a switch turned off in
     * CUSTOMIZE takes its flag away, a cheat typed stays for the session and is never written to the mode's file (the
     * switch is the player's default, not a veto on the cheat); mirror, rally and Lost Vegas are the level's alone */
    extras_settle_to(&e, own ? edition_expect(&central_edition) : extras_expect_level(cg_g.unlock_level), &f, &extras_session);
    if (cg_g.unlock_level < 2) reverse_track_pick(0);   /* port: the way round (INVERSE) is level 2's, as the rally */
    /* cheat_codes writes the globals, so they are read back and put into g again after the reapply */
    cg_g.unlock_m = *f.unlock_m, cg_g.unlock_r = *f.unlock_r, cg_g.variant_m = *f.variant_m, cg_g.variant_r = *f.variant_r;
    cg_g.explode = *f.explode, cg_g.nitro = *f.nitro, cg_g.vegas_cheat = *f.vegas_cheat, cg_g.lost_vegas = *f.lost_vegas;
    cg_g.warrior = *f.warrior;
    menu_cheats.keys_506 = (int32_t)*f.track_flags;
    if (e.no_assists) extras_no_assists(&cg_g.car_flag_a, &cg_g.car_flag_b);   /* after 00465ec8 */
    warrior_rival_flag = (int16_t)(e.warrior_rival != 0);
    warrior_keep_flag = (int16_t)(e.warrior != 0);
    restomod_flag = (int16_t)(port_settings_profile() == PROFILE_RESTOMOD);
    car_pieces_follow();   /* port (dep59): the added cars of the mods in effect, for the arrows (car_list.h); loaded again only when they change */
    cg_variant_choice_apply();   /* port (dep65): Restomod's switches back from the player's choice, after every place they are cleared */
    cg_lunar_settle();
}

/* 00465ec8, 00432974, 00432c40, 00432d48 and 00432720 on the shorts */
static void car_flags(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    s.player_car = cg_g.player_car;   /* port (dep59): the car picked, an added one too (race_settings_car_flags takes it) */
    race_settings_car_flags(&s);
    cg_settings_put(&s);
    cg_extras_apply();   /* port: after 00465ec8 / 00432974 */
}

void cg_tournament_variants_apply(void)
{
    if (cg_g.race_type != RACE_TOURNAMENT) return;
    TournamentVariants v;
    if (!tournament_variants_settings(tournament_variants_get(), cg_g.track, variant_switches_on(), cg_g.unlock_m, cg_g.unlock_r,
                                      cg_g.unlock_level, &v))
        return;
    cg_g.variant_m = v.mirror, cg_g.variant_r = v.rally;
    reverse_track_pick(v.inverse);
}

/* port (dep56): a Restomod tournament entered from another race type takes the switches as they were as its variants
 * (race_settings_type_changed clears them, as the exe's 00432974), unless its races have begun (a game loaded); either
 * way the tournament's are put back on */
static void tournament_entered(int was_type, int was_m, int was_r)
{
    if (variant_switches_on() && cg_g.race_type == RACE_TOURNAMENT && was_type != RACE_TOURNAMENT && !tournament_variants_locked(cg_g.races))
        tournament_variants_set((TournamentVariants){ was_m != 0, reverse_track_picked(), was_r != 0 });
    cg_tournament_variants_apply();
}

static void type_changed(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    const int was_type = s.race_type, was_m = s.variant_m, was_r = s.variant_r;
    Unlocks u = cg_unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_changed(&s, &u, &f);
    cg_settings_put(&s), cg_unlocks_put(&u), session_put(&f);
    cg_extras_apply();   /* port: the type change clears the variants */
    tournament_entered(was_type, was_m, was_r);
}

/* port (dep59 step 4): Control Central opening on a tournament (config.dat's, or a game loaded) in an edition that closes it opens on
 * Time Trial (the tournament no longer under way; its game state is left as it is); so does one loaded in Options */
static void tournament_leave_closed(void)
{
    if (cg_g.race_type != RACE_TOURNAMENT || !tournament_closed()) return;
    port_log("mark:race_type=no_tournament\n");
    cg_g.race_type = RACE_TIME_TRIAL, cg_g.tournament = 0;
    type_changed(NULL);
}

/* port (dep59 step 4): the edition's tracks Control Central's arrows walk (track_list.h), into `l`; NULL in the standard edition */
static const TrackList *edition_tracks(TrackList *l)
{
    if (central_edition.tracks == 0) return NULL;
    track_list_build(l, &central_edition, 0, TRACK_LIST_LUNAR_NEVER);
    track_list_stock_only(l);   /* port (dep59 step 5): Best Times' tables are the original's nine (an added track's are T7e's) */
    return l;
}

/* port (dep59 Lunar L2): 1 when the track list offers Lunar Springs: the standard edition's switch at level 2, an edition's lunar in
 * tracks= (unlock_level_gate); never in a tournament */
static int lunar_offered(void)
{
    if (cg_g.race_type == RACE_TOURNAMENT) return 0;
    return unlock_level_gate(port_extras_load(), cg_g.unlock_level, central_edition.cars > 0 ? &central_edition : NULL).lunar_springs != 0;
}

/* port (dep59 Lunar L2): the tracks Control Central's arrows walk, Lunar among them when offered (track_pick.h); NULL for the standard
 * edition's own walk without it */
static const TrackList *central_tracks(TrackList *l)
{
    const int lunar = lunar_offered() ? TRACK_LIST_LUNAR_OFF : TRACK_LIST_LUNAR_NEVER;
    if (central_edition.tracks > 0) {
        track_list_build(l, &central_edition, 0, lunar);
        return l;
    }
    if (lunar == TRACK_LIST_LUNAR_NEVER) return NULL;
    track_list_build(l, NULL, cg_g.lost_vegas != 0, TRACK_LIST_LUNAR_ON);
    return l;
}

/* port (dep59 Lunar L5): the list the track screen's arrows walk with Lunar (track_list.h's TRACK_LIST_LUNAR_*) */
static int lunar_list_of(void)
{
    return !lunar_offered() ? TRACK_LIST_LUNAR_NEVER : central_edition.tracks > 0 ? TRACK_LIST_LUNAR_OFF : TRACK_LIST_LUNAR_ON;
}

/* port (dep59 Lunar L5): the screens' banks, Lunar Springs' made art for its names (track_select.h's LUNAR_SELECT_FILE, best_times.h's
 * LUNAR_BEST_FILE): Rusty's when it cannot be made (logged by lunar_art_bank) */
static int32_t lunar_or_bank(void *ctx, const char *path, int32_t flags)
{
    {   /* port (dep59 step 5, T7e2): an added track's Best Times art, Coastal's when it cannot be made (logged by added_best_art_bank) */
        const char *slash = strrchr(path, '/');
        const char *file = slash != NULL ? slash + 1 : path;
        const size_t n = strlen(ADDED_BEST_FILE);
        if (strncmp(file, ADDED_BEST_FILE, n) == 0 && strcmp(file + n, ".qfs") == 0) {
            const int32_t bank = added_best_art_bank();
            if (bank != 0) return bank;
            char coast[256];
            snprintf(coast, sizeof coast, "%.*scoastbst.qfs", (int)(file - path), path);
            return load_bank(ctx, coast, flags);
        }
    }
    static const struct { const char *name, *rusty; LunarArtKind kind; } MOON[] = {
        { LUNAR_SELECT_FILE, "rustsel", LUNAR_ART_SELECT }, { LUNAR_BEST_FILE, "rustbst", LUNAR_ART_BEST } };
    for (size_t i = 0; i < sizeof MOON / sizeof MOON[0]; i++) {
        const char *slash = strrchr(path, '/');
        const char *file = slash != NULL ? slash + 1 : path;
        const size_t n = strlen(MOON[i].name);
        if (strncmp(file, MOON[i].name, n) != 0 || strcmp(file + n, ".qfs") != 0) continue;
        const int32_t bank = lunar_art_bank(MOON[i].kind);
        if (bank != 0) return bank;
        char rusty[256];
        snprintf(rusty, sizeof rusty, "%.*s%s.qfs", (int)(file - path), path, MOON[i].rusty);
        return load_bank(ctx, rusty, flags);
    }
    return load_bank(ctx, path, flags);
}

static int32_t lunar_or_art(void *ctx, const char *name) { return lunar_or_bank(ctx, name, 0); }

/* port (dep59 Lunar L8): this machine joined a host's link game (link_race.c: intro_seen 0 is a guest) */
static int link_guest(void) { return menu_words.link_game != 0 && menu_words.intro_seen == 0; }

/* port (dep59 Lunar L2): the pick holds only on Rusty's rally where Lunar is offered, and L8: never for a guest in a link game */
void cg_lunar_settle(void)
{
    if (!track_pick_holds(lunar_offered(), cg_g.track, cg_g.variant_r, link_guest())) track_pick_lunar_put(0);
}

/* port (dep59 Lunar L2): the arrows over the entries, Lunar's pick with them */
static void track_arrow(int dir)
{
    RaceSettings s = cg_settings_of();
    const Unlocks u = cg_unlocks_of();
    TrackList l;
    int lunar = track_pick_lunar();
    /* port (dep56): with the switches the variants are the track screen's */
    track_pick_step(&s, &lunar, &u, central_tracks(&l), dir, variant_switches_on());
    cg_settings_put(&s);
    track_pick_lunar_put(lunar);
    cg_variant_choice_apply();   /* port (dep65): the choice on the new track (Lost Vegas takes no mirror or rally, the next does) */
    cg_tournament_variants_apply();   /* a tournament's are its own on every track (Lost Vegas: none) */
    cg_lunar_settle();
}

static void track_forward(void *ctx)
{
    (void)ctx;
    track_arrow(1);
}

static void track_backward(void *ctx)
{
    (void)ctx;
    track_arrow(-1);
}

/* port (dep65): the card's words in the log when they change, "mark:card=<track> <left words>|<right words>", each list joined by
 * '+' (an input script's steps are split at commas), for the walks that check what Control Central says after a screen or a step
 * (tests/variant_choice_kept.cmake) */
static void card_marks_log(const CardWords *w, int track)
{
    static char logged[128];
    char now[128];
    int n = snprintf(now, sizeof now, "%d ", track);
    for (int i = 0; i < w->left_count && n > 0 && (size_t)n < sizeof now; i++)
        n += snprintf(now + n, sizeof now - (size_t)n, "%s%s", i > 0 ? "+" : "", w->left[i]);
    for (int i = 0; i < w->right_count && n > 0 && (size_t)n < sizeof now; i++)
        n += snprintf(now + n, sizeof now - (size_t)n, "%s%s", i > 0 ? "+" : "|", w->right[i]);
    if (strcmp(now, logged) == 0) return;
    snprintf(logged, sizeof logged, "%s", now);
    port_log("mark:card=%s\n", now);
}

/* port (dep56, dep64): Restomod's track card (card_marks.h): its words from the settings as they are at this draw, at half size,
 * one line each down from `y` (a line's height at that size and a pixel between) */
static void card_marks(void *ctx, int32_t left, int32_t right, int32_t y)
{
    (void)ctx;
    if (!variant_switches_on()) return;
    enum { LINE = 9 };
    const RaceSettings s = cg_settings_of();
    const int own = port_records_unified() ? port_settings_load().laps : 0;   /* the laps row's custom count (dep55) */
    /* port (dep59 Lunar L5): Lunar's card says no rally (it is one), clear and by day; its mark names the entry, 10 */
    const int lunar = track_pick_of(s.track, s.variant_r, track_pick_lunar()) == STOCK_TRACK_LUNAR;
    const CardWords w = card_words_of(s.track, s.variant_m, reverse_track_picked(), s.variant_r && !lunar, lunar ? WEATHER_CLEAR : port_weather(),
                                      port_light_effects() ? (lunar ? LIGHT_TIME_DAY : port_light_time()) : -1, own > 0 ? own : s.laps);
    card_marks_log(&w, lunar ? STOCK_TRACK_LUNAR : s.track);
    text_select(TEXT_YELLOW);
    for (int i = 0; i < w.left_count; i++) text_draw_smooth(w.left[i], left, y + i * LINE, 1, 2);
    for (int i = 0; i < w.right_count; i++) text_draw_smooth(w.right[i], right - text_width_scaled(w.right[i], 1, 2), y + i * LINE, 1, 2);
}

static int card_plain(void *ctx)
{
    (void)ctx;
    return variant_switches_on();
}

static int quit_tournament(int german, void *ctx)
{
    return glue_yes_no(ctx, german != 0 ? "Wettkampf beenden ?" : "Quit tournament ?");
}

static void step_race_type(void *ctx, int32_t cg_key)
{
    RaceSettings s = cg_settings_of();
    const int was_type = s.race_type, was_m = s.variant_m, was_r = s.variant_r;
    Unlocks u = cg_unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_step(&s, &u, &f, cg_game_state, config_current(), cg_key, quit_tournament, ctx);
    cg_settings_put(&s), cg_unlocks_put(&u), session_put(&f);
    if (s.race_type == RACE_TOURNAMENT && was_type != RACE_TOURNAMENT) cg_races_from_state();   /* a new tournament's: none run */
    tournament_entered(was_type, was_m, was_r);
}

/* 00432884 on the art's handles */
static void art_free(void *ctx)
{
    ControlCentralArt a;
    void **const slot[7] = { &a.art_4bafc0, &a.art_4bafc4, &a.art_4bafc8, &a.art_4bafcc, &a.art_4bafd0, &a.art_4bafd4,
                             &a.art_4bafd8 };
    for (int i = 0; i < 7; i++) *slot[i] = (void *)(intptr_t)cg_g.art[i];
    FrontendSession f = session_of();
    const ControlCentralArtOps ops = { free_handle, nothing, ctx };
    control_central_art_free(&a, &f, &ops);
    for (int i = 0; i < 7; i++) cg_g.art[i] = (int32_t)(intptr_t)*slot[i];
    session_put(&f);
}

/* Control Central holds 0052b316 and 004f81d2 as values, but in the original they are live words that 004104d0
 * changes (its 004465e4 rebuilds the seats as machines join): read again after each call while Central runs */
static ControlCentralState *central_state;
void cg_net_service(void *ctx)
{
    (void)ctx, link_sync();
    if (central_state != NULL) central_state->view_only = link_view_only(), central_state->players = link_humans();
}
/* 004baf44..: the codes' letters with 0x63 added, NUL after each; 004baf90..: each one's position, in
 * ControlCentral's order (00434d69..00434f93 pass them in pairs); LUNAR's (004baf7f, 004bafac) is not stepped there */
static const uint8_t CHEAT_LETTERS[] = {
    0xb6, 0xae, 0xa6, 0xa4, 0xb5, 0xb7, 0x00, 0xa8, 0xa4, 0xb5, 0xa4, 0xaf, 0xaf, 0xbc, 0xa8, 0x00, 0xa8, 0xbb, 0xb3,
    0xaf, 0xb2, 0xa7, 0xa8, 0x00, 0xa6, 0xa4, 0xb6, 0xac, 0xb1, 0xb2, 0xb9, 0xac, 0xaf, 0xaf, 0xa8, 0x00, 0xa6, 0xb2,
    0xb1, 0xa6, 0xa8, 0xb3, 0xb7, 0xa6, 0xa4, 0xb5, 0x00, 0xb7, 0xb8, 0xb5, 0xa5, 0xb2, 0x00, 0xb1, 0xac, 0xb7, 0xb5,
    0xb2, 0x00, 0xaf, 0xb8, 0xb1, 0xa4, 0xb5, 0x00, 0xb5, 0xb2, 0xa6, 0xae, 0xbc, 0xb5, 0xb2, 0xa4, 0xa7, 0x00
};
static const uint8_t CHEAT_AT[CENTRAL_CHEATS] = { 0x00, 0x07, 0x10, 0x35, 0x18, 0x24, 0x2f, 0x41 };  /* - 004baf44 */
static int32_t cheat_pos[CENTRAL_CHEATS];  /* 004baf90, 94, 98, a8, 9c, a0, a4, b0 */

static int32_t cheat_step(void *ctx, int which)
{
    (void)ctx;
    if (which < 0 || which >= CENTRAL_CHEATS) return 0;
    return cheat_code_step(&cheat_pos[which], CHEAT_LETTERS + CHEAT_AT[which], keys);
}

/* 00483f8c */
static int32_t key_down(void *ctx, int32_t scancode) { (void)ctx; return keys[(uint32_t)scancode & 0xff]; }
/* port: the screen palette's index nearest an RGB (ea_lib.c), for the car select's Color line's swatch */
static int32_t palette_colour(void *ctx, int32_t rgb)
{
    (void)ctx;
    return ea_colour(rgb);
}
/* port: the pad's virtual buttons this frame (control_map.h), for the car select's Color line */
static int32_t pad_buttons(void *ctx)
{
    uint32_t buttons = 0;
    int32_t axis[4] = { 0, 0, 0, 0 };
    (void)ctx;
    return platform_joy_read(0, &buttons, axis) ? (int32_t)buttons : 0;
}

/* config.dat's short at 00524df0 + at */
int16_t cg_image_short(int at)
{
    int16_t v;
    memcpy(&v, config_current()->image + at, sizeof v);
    return v;
}

/* config.dat's 00524e65 / 67 / 69 as g.volumes for the screens that put back the volumes F5 kept
 * (menu_audio_kept's +0, +4, +6) when 004c5cbe is set: before and after each, what a screen changed there goes
 * to config.dat, then g.volumes is read again (F5 itself writes config.dat) */
enum { VOLUMES_AT = 0x75 };
static int16_t volumes_read[3];
void cg_volumes_sync(void)
{
    uint8_t *const at = config_current()->image + VOLUMES_AT;
    if (memcmp(cg_g.volumes, volumes_read, sizeof cg_g.volumes) != 0) memcpy(at, cg_g.volumes, sizeof cg_g.volumes);
    memcpy(cg_g.volumes, at, sizeof cg_g.volumes);
    memcpy(volumes_read, cg_g.volumes, sizeof volumes_read);
}

void cg_palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx;
    palette_put(rgb, first, count, src, mode, cg_screen_palette);
}
/* 00464a30: both pages black (screen_clear_both) */
void cg_flush(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}

/* The short 0050a5b6 is the link's stage word (0050a5ac[5], race_net_on_stage): a client's becomes 1 when the host
 * reaches 00410c18(1), and every screen of the menus leaves on it. The original's service thread keeps it current; here
 * each screen's loop calls its deadline op first, which serves the link and, for the screens that took the word by
 * value in their inputs, writes it there */
int *cg_stage_in;  /* the running screen's inputs' copy of it (results_screen.h, standings.h) */
int32_t cg_deadline_passed(void *ctx)
{
    (void)ctx;
    if (menu_words.link_game != 0) {
        link_service();
        if (cg_stage_in != NULL) *cg_stage_in = *stage_word();
    }
    return timer_deadline_passed();
}
int32_t cg_key(void *ctx)
{
    (void)ctx;
    for (int code = 0; code < CHEAT_KEY_FLAGS; code++) keys[code] = platform_key_down(code) ? 1 : 0;
    return input_get_key();
}

/* 004687d0's pointer: ctx is its DialogPointer */
void cg_poll_pointer(void *ctx)
{
    DialogPointer *const p = ctx;
    int x, y;
    pointer_sync_from_os();
    pointer_position(&x, &y);
    p->x = x, p->y = y;
}
void cg_mark_at(void *ctx, DialogPointer *p)
{
    (void)ctx;
    p->mark_x = (int16_t)p->x, p->mark_y = (int16_t)p->y;
    pointer_mark();
}

/* 004fbae0: a dword per track; 00437540 sets the nine to 1 (midday) as the game starts (00481f27) */
int32_t cg_time_of_day[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* 00465ec8 on config.dat's image, which 004687d0 changes */
void cg_image_car_flags(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    RaceSettings s = cg_settings_of();
    if (!config_dat_get_race(config, &s)) return;
    race_settings_car_flags(&s);
    if (port_extras_load().no_assists) {   /* port: no driving aids */
        int16_t abs = (int16_t)s.car_flag_a, traction = (int16_t)s.car_flag_b;
        extras_no_assists(&abs, &traction);
        s.car_flag_a = abs, s.car_flag_b = traction;
    }
    config_dat_set_race(config, &s);
}

/* 004687d0 on config.dat's image (00524df9..), the globals written to it first and read back after */
static int32_t best_times(void *ctx);
/* port (dep59 step 5, T7e2): the added track Best Times is on (BestTimesInputs.added), 0 none; cg_best_times_added sets it */
static int best_added;
/* port (dep56): the track screen's way round while it is open (its Inverse or Reverse row), so Best Times opened from it reads the
 * layout the screen shows; NULL when the screen is closed or offers none. The pick itself is stored as the screen closes. */
static const int *track_screen_reverse;
/* port (dep59 Lunar L5): the track screen's Lunar pick while it is open, so Best Times opened from it shows the entry it shows */
static int *track_screen_lunar;
static int32_t plus_text_width(void *ctx, const char *text);
static void track_select(void *ctx)
{
    (void)ctx;
    if (track_registry_is_added(cg_g.track)) {   /* port (dep59 step 5): this screen's tables are the original's nine; an added track's page is T7d's */
        port_log("track screen: track %d is an added track: its page is not made yet\n", cg_g.track);
        return;
    }
    ConfigDat *const config = config_current();
    const RaceSettings before = cg_settings_of();
    port_log("mark:screen=track_select\n");   /* port: the headless walk (tools/menu_walk.sh) waits for it */
    config_dat_set_race(config, &before);
    cg_tournament_variants_apply();   /* port (dep56): a tournament's variants on its switches */
    const int tournament = variant_switches_on() && cg_g.race_type == RACE_TOURNAMENT;
    const int32_t screen = ea_screen();
    const TrackSelectInputs in = { "", resource_kmh(), cg_g.variant_m, cg_g.variant_r, cg_g.lost_vegas, screen,
                                   ea_picture_shape(screen), &central_edition };   /* port (dep59 step 4): its tracks the arrows walk */
    /* port: the weather row, and with the light effects the time-of-day item on its light_time; both written to port.cfg
     * when the screen leaves them changed (the race reads them there, as it does today) */
    int weather = port_weather(), light_time = port_light_time();
    int reverse = reverse_track_picked();   /* port: the mode's file's reverse offers the row; Restomod keeps the pick as its choice (dep65) */
    int laps = port_settings_load().laps;   /* port (dep55): Restomod's custom laps on the laps row, written back to port.cfg */
    const int was_laps = laps;
    const PortExtras extras = port_extras_load();
    const int reverse_offered = cg_reverse_offered();
    const PortSettings choice = port_settings_load();   /* port (dep65): the switches' choice, kept through Lost Vegas */
    TrackSelectPlus plus = { cg_g.unlock_m, cg_g.unlock_r, &cg_g.variant_m, &cg_g.variant_r,
                             extras.variant_time,   /* port: variants as entries */
                             port_settings_profile() == PROFILE_RESTOMOD, &weather, port_light_effects() ? &light_time : NULL, reverse_offered ? &reverse : NULL,
                             port_records_unified() ? &laps : NULL, variant_switches_on(), tournament && tournament_variants_locked(cg_g.races),
                             choice.variant_mirror, choice.variant_rally, NULL, lunar_list_of() };
    int lunar = track_pick_lunar();   /* port (dep59 Lunar L5): the screen's entry, Lunar among them */
    plus.lunar = plus.lunar_list != TRACK_LIST_LUNAR_NEVER ? &lunar : NULL;
    const int was_weather = weather, was_light_time = light_time;
    TrackSelectState st = { config->image + (0x524df9 - 0x524df0), cg_time_of_day, cg_g.inverse, cg_screen_palette, &plus };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const TrackSelectOps ops = {
        cg_flush, move_to, lunar_or_bank, no_inverse, music_feed, shape, set_palette, nothing, styles, cg_poll_pointer, hide,
        in_rect, select_picture, select_default, blit, blit_masked, blit_word, style, shadowed, nothing, nothing, flip,
        show_dialog, cg_mark_at, buttons, track_music, fade_in, track_play, deadline_set, sound, ticks, activate_dialog,
        cg_key, click, track_stop, track_free, fade_out, free_bank, free_handle_inverse, best_times, cg_image_car_flags,
        off_dialog, music,
        cg_deadline_passed, music_fade_out, music_end, &p };
    track_screen_reverse = reverse_offered ? &reverse : NULL;
    track_screen_lunar = &lunar;
    track_select_screen(&in, &st, &p, &ops);
    track_screen_reverse = NULL, track_screen_lunar = NULL;
    if (reverse_offered) reverse_track_pick(reverse);
    if (weather != was_weather || light_time != was_light_time || laps != was_laps) {
        PortSettings saved = port_settings_load();
        if (weather != was_weather) saved.weather = weather;
        if (light_time != was_light_time) saved.light_time = light_time;
        if (laps != was_laps) saved.laps = laps;
        if (!port_settings_save(&saved)) port_log("track select: port.cfg could not be saved\n");
    }
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) {
        after.variant_m = cg_g.variant_m, after.variant_r = cg_g.variant_r;   /* port: as the screen left them (get_race zeroes them) */
        cg_settings_put(&after);
    }
    track_pick_lunar_put(lunar);
    cg_lunar_settle();
    cg_variant_choice_keep(plus.kept_m, plus.kept_r, reverse_offered ? &reverse : NULL);   /* port (dep65): Restomod keeps them */
    if (tournament && !tournament_variants_locked(cg_g.races))   /* port (dep56): before its first race, the switches are its variants */
        tournament_variants_set(tournament_variants_follow(tournament_variants_get(), cg_g.track, cg_g.variant_m,
                                                           reverse_offered ? reverse : tournament_variants_get().inverse, cg_g.variant_r));
    cg_tournament_variants_apply();
}

void cg_fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)ctx;
    static const TextFitOps ops = { fit_width, fit_ramp, fit_draw, NULL };
    text_fit_shadowed(text, x, y, width, &ops);
}
void cg_band_text(void *ctx, const char *text, int32_t x, int32_t y_mid, int32_t room)
{
    (void)ctx;
    static const int SCALES[][2] = { { 1, 1 }, { 7, 8 }, { 3, 4 }, { 2, 3 }, { 1, 2 } };
    enum { SCALE_COUNT = (int)(sizeof SCALES / sizeof SCALES[0]) };
    int k = 0;
    while (k < SCALE_COUNT - 1 && text_width_scaled(text, SCALES[k][0], SCALES[k][1]) > room) k++;
    const int height = text_line_height() * SCALES[k][0] / SCALES[k][1];
    text_draw_smooth(text, x, y_mid - height / 2, SCALES[k][0], SCALES[k][1]);
}
void cg_average_text(void *ctx, char *out, int32_t time, int32_t distance)
{
    (void)ctx;
    stat_average_speed_text(out, resource_kmh(), time, distance);
}

/* port (dep55): the letters' rows, for the records' conditions block */
static void letters_ink(void *ctx, const char *text, int32_t num, int32_t den, int32_t *top, int32_t *bottom)
{
    (void)ctx;
    int t, b;
    text_ink_scaled(text, num, den, &t, &b);
    *top = t, *bottom = b;
}

/* 004676d8 from the track screen on config.dat's image and its records; ctx is the track screen's pointer */
static int32_t best_times(void *ctx)
{
    uint8_t *const image = config_current()->image;
    const int32_t screen = ea_screen();
    port_log("mark:screen=best_times\n");   /* port: the headless walk (tools/menu_walk.sh) waits for it */
    if (track_screen_reverse != NULL) reverse_track_pick(*track_screen_reverse);   /* port (dep56): the screen's layout as it shows it */
    TrackList l;
    int lunar_own = track_pick_lunar();   /* port (dep59 Lunar L5): the entry's pick, the track screen's while it is open */
    int *const lunar = track_screen_lunar != NULL ? track_screen_lunar : &lunar_own;
    const BestTimesInputs in = { "", resource_german(), cg_g.lost_vegas, screen, ea_picture_shape(screen),
                                 image + (0x524f8f - 0x524df0), edition_tracks(&l), lunar, &best_added };
    BestTimesState st = { image + (0x524df9 - 0x524df0), cg_g.inverse, cg_screen_palette,
                          records_glue_best_plus(image, cg_g.variant_m, cg_g.variant_r, cg_g.explode, cg_g.nitro, (menu_cheats.keys_506 & 8) != 0,
                                                 cg_g.player_car) };   /* port (dep55): Restomod's pages; NULL the exe's screen */
    const BestTimesOps ops = {
        move_to, lunar_or_art, no_inverse, music_feed, shape, cg_flush, set_palette, nothing, styles, cg_poll_pointer, cg_mark_at,
        in_rect, select_picture, blit, blit_masked, blit_word, style, cg_fit, time_text, speed_text, shadowed,
        select_default, hide, nothing, nothing, flip, show_dialog, buttons, fade_in, deadline_set, sound, ticks,
        activate_dialog, cg_key, click, cg_image_car_flags, fade_out, free_handle_inverse, free_bank, lunar_or_bank, off_dialog,
        music,
        cg_deadline_passed, music_fade_out, music_end, ctx, cg_plus_text, plus_text_width, glue_plus_text_scaled, glue_plus_text_width_scaled,
        letters_ink };   /* port (dep55): the pages' words */
    input_page_keys(st.plus != NULL);   /* port (dep55): Page Up / Page Down turn the pages */
    const int32_t result = best_times_screen(&in, &st, ctx, &ops);
    input_page_keys(0);
    if (track_screen_lunar == NULL) {   /* port (dep59 Lunar L5): opened from Control Central, the pick is put back as it left it */
        track_pick_lunar_put(lunar_own);
        cg_lunar_settle();
    }
    return result;
}
int32_t cg_best_times_added(void *ctx, int track)
{
    uint8_t *const word = config_current()->image + (0x524df9 - 0x524df0) + TRACK_SELECT_TRACK;   /* the screen's track word, a short */
    const uint8_t was_low = word[0], was_high = word[1];
    const int was_added = best_added;
    word[0] = 1, word[1] = 0;   /* Coastal's stage layout, the added track's (best_times.h) */
    best_added = track_registry_is_added(track) ? track : 0;
    const int32_t result = best_times(ctx);
    best_added = was_added;
    word[0] = was_low, word[1] = was_high;
    return result;
}
/* Cancel or Escape: the settings back as they were on entry, and the config.dat image with them -- the view distance
 * is one of the screen's shorts there, not in port.cfg, so the kept image is what puts it back. Both are kept for as
 * long as the screen may be up, which is what the file scope is for. */
static PortSettings port_settings_was;
static uint8_t config_current_kept_image[GRAPHICS_KEPT];

static void own_put_back(void *ctx, PortSettings *settings)
{
    (void)ctx;
    *settings = port_settings_was;
    memcpy(config_current()->image + 9, config_current_kept_image, GRAPHICS_KEPT);
}

/* port: the Graphics screen (graphics_own.c) from Options (mode 0, with the second page) and from the race's pause menu
 * (mode 1, GraphicsPlus.race: the rows the next race reads are dimmed there). Both edit port.cfg; the view distance is
 * config.dat's own short, which the race's menu takes back through its image. The race applies what it can when the
 * menu closes (drive_menu.c, track_viewer_graphics_apply). */
static void graphics(void *ctx, int32_t mode)
{
    static int16_t showing;  /* 004f9e8c */
    const int cg_race = mode != 0;
    PortSettings port = port_settings_load();
    const PortSettings was = port;
    int desktop_w = 0, desktop_h = 0;
    const int has_desktop = platform_desktop_size(&desktop_w, &desktop_h);
    GraphicsPlus plus = { &port, has_desktop ? screen_format_nearest(desktop_w, desktop_h) : SCREEN_4_3, cg_race, port.f3_ultra };
    cg_opt_in();
    /* after opt_in: it writes Options' unsaved edits into the image, which Cancel must keep */
    port_settings_was = port;
    memcpy(config_current_kept_image, config_current()->image + 9, GRAPHICS_KEPT);
    /* port: the palette that brought this screen up, kept: it fades the whole display out as it closes (00423d28's
     * close_screen), so the styles rebuilt after it must be built from this copy rather than from what is left on the
     * display -- which is that palette faded down to black, and a ramp built from black is not the caller's menu. */
    uint8_t caller_palette[PALETTE_PUT_BYTES];
    memcpy(caller_palette, cg_screen_palette, sizeof caller_palette);
    const GraphicsMenuState st = { config_current()->image + 9, &showing, cg_word_52b302(), pointer_active_word(), &cg_g.buttons,
                                   ea_screen_slot(), &cg_g.inverse, stage_word(), &plus };
    const GraphicsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, poll, mark, in_rect, select_picture, blit,
        picture_shape, select_default, hide, nothing, flip, show, buttons, styles, fade_in, deadline_set, sound,
        nothing, ticks, activate, cg_key, click, off, music, net_sync, cg_deadline_passed, fade_out, free_handle_inverse,
        free_bank, ctx, cg_plus_text, plus_text_width, cg_plus_panel, cg_plus_save,
        /* port: the screen's own background, its shape with the palette that goes with it, and its title. The race's
         * pause menu gets the same ones as Options. */
        cg_own_bgnd, cg_own_bgnd_shape, own_title_text, own_put_back,
        /* port: the letters go through the draw that restores the clean background under them */
        own_plus_text,
        /* port: whether the display is already dark, so the screen skips a fade of a display Options has already
         * faded out rather than scaling zeros for seventeen vblanks */
        cg_display_dark, central_page_arrow_size, central_page_arrow };
    input_function_keys(&showing);
    input_page_keys(!cg_race);
    /* port: graphics_own.c, the port's own screen, from Options and from the race's pause menu alike */
    graphics_own(&st, &ops);
    /* the screen put its own palette on and faded the display out as it left; the one that brought it up goes
     * back, so whatever is on screen afterwards is drawn the way its own caller means */
    text_build_styles_from(caller_palette);
    input_page_keys(0);
    input_function_keys(NULL);
    cg_opt_out();
    /* port: the screen's settings changed (Done): the window takes them now, not at the next start */
    const PortSettings now = port_settings_load();
    if (mode == 0 && (was.format != now.format || was.scale_integer != now.scale_integer
                      || was.fullscreen != now.fullscreen || was.refresh_hz != now.refresh_hz)) {
        const PlatformConfig config = port_platform_config(NULL);
        if (!platform_reconfigure(&config)) port_log("graphics: the window keeps its old settings\n");
    }
}

/* check_options (00403760) on config.dat's image (the sub-screens' shorts taken from it first): the options after a race, mode
 * 0 from 0040b648, 1 from 0040c710 */
int32_t glue_check_options(int32_t mode)
{
    ConfigDat *const config = config_current();
    cg_opt_out();
    const CheckOptionsOps ops = {
        move_to, load_art, no_inverse, shape, cg_palette_set, poll, mark, in_rect, select_picture, blit, select_default,
        hide, picture_shape, flip, show, buttons, fade_in, deadline_set, sound, nothing, ticks, activate, cg_key, click,
        fade_out, free_handle_inverse, free_bank, cg_audio, graphics, cg_controls, cg_load_save, cg_load_save2, music_fade_to,
        off, music, cg_deadline_passed, no_wait, music_feed, save_config, NULL };
    const int16_t language = (int16_t)resource_german();
    const CheckOptionsState st = { config->image, &menu_words.link_game, &language, pointer_active_word(), &cg_g.buttons,
                                   ea_screen_slot(), &cg_g.inverse, stage_word() };
    const int32_t done = check_options(mode, &st, &ops);
    cg_opt_out();
    return done;
}

/* 00447564's option screens from the race's pause menu, on config.dat's image: 004015dc with eax 2 (audio), 00423d28
 * with eax 1 (graphics, no resolution) */
void glue_race_option(uint32_t address)
{
    if (address == 0x4015dc) cg_audio(NULL, 2);
    else if (address == 0x423d28) graphics(NULL, 1);
}

/* port: the Extras row opens the EXTRAS screen over this screen's picture; Done writes the mode's file and the unlock
 * level back into config.dat, and extras_apply re-reads it for Control Central (the cheats, mirror, ghosts). */
static void options_extras(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    glue_extras_screen((int16_t)unlock_level_get(config));
    cg_g.unlock_level = (int16_t)unlock_level_get(config);
    cg_extras_apply();
}

static void options(void *ctx)
{
    ConfigDat *const config = config_current();
    const RaceSettings before = cg_settings_of();
    config_dat_set_race(config, &before);
    cg_opt_out();
    const OptionsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, styles, poll, mark, in_rect, select_picture,
        blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in, deadline_set, sound,
        ticks, activate, cg_key, click, fade_out, free_handle_inverse, free_bank, cg_audio, graphics, cg_controls, cg_load_save,
        music_fade_out, music_end, music_start, cg_credits, music_pick, cg_opt_car_flags, music_fade_to, off, music, net_sync,
        cg_deadline_passed, ctx, options_extras, central_options_plus_text, plus_text_width, cg_plus_panel };
    OptionsMenuState st = {
        &cg_opt[OPT_TRANSMISSION], &cg_opt[OPT_AUTOMATIC], &cg_opt[OPT_ABS], &cg_opt[OPT_TRACTION], &cg_opt[OPT_RACE_TYPE],
        &cg_opt[OPT_TRACK], &cg_opt[OPT_OPPONENT], &cg_opt[OPT_CAR], &cg_opt[OPT_NET_CAR], &cg_opt[OPT_LAPS], &cg_opt[OPT_DIFFICULTY],
        &cg_opt[OPT_MUSIC_VOLUME], &menu_words.link_game, pointer_active_word(), &cg_g.buttons, ea_screen_slot(), &cg_g.inverse,
        cg_word_52b302(), stage_word(), &menu_words.redraw };
    options_menu(&st, &ops);
    cg_opt_in();
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) cg_settings_put(&after);
    tournament_leave_closed();   /* port (dep59 step 4): a tournament game loaded in an edition that closes it */
    cg_variant_choice_apply();   /* port (dep65): get_race zeroes the variants; Restomod's come back from the player's choice */
    cg_tournament_variants_apply();   /* port (dep56): get_race zeroes the variants; a tournament (one loaded too) runs its own */
}

/* config.dat's race settings and unlock level into the globals, as the draft read them */
static void load(void)
{
    ConfigDat *const config = config_current();
    RaceSettings s;
    if (!config_dat_get_race(config, &s)) {
        ConfigDat defaults;
        config_dat_defaults(&defaults, resource_german());
        config_dat_get_race(&defaults, &s);
    }
    cg_settings_put(&s);
    cg_g.unlock_level = (int16_t)unlock_level_get(config);
    cg_races_from_state();
}

/* 00466ae4 before Control Central: the game state for a new tournament (the controls, the names: config.dat's for
 * player 0, the computer players' for the rest), the race settings read from config.dat and put back to it */
void glue_game_reset(void)
{
    load();
    RaceSettings s = cg_settings_of();
    FrontendSession f = session_of();
    tournament_new(cg_game_state, &s, config_current(), &f);
    cg_settings_put(&s);
    session_put(&f);
    tournament_variants_reset();   /* port (dep56): a new tournament's are the base until chosen */
    config_dat_set_race(config_current(), &s);
    /* the original's settings are config.dat's own words, so tournament_reset's variant = 0 clears 00524e83 */
    config_dat_new_session(config_current());
}

/* 00408724 on Control Central's words */
static void showcase(void *ctx)
{
    const ShowcaseGlue in = { &cg_g.player_car, &cg_g.race_type, &cg_g.track, pointer_active_word(), &cg_g.buttons, car_flags, ctx, NULL };
    glue_showcase(&in);
}

/* port (dep59 section 4): ControlCentralOps.showcase_carousel: the showcase leaving on its Next / Previous */
static int32_t showcase_carousel(void *ctx, int entry)
{
    int16_t step = (int16_t)entry;   /* in: the step it is entered by; out: the one it leaves on */
    const ShowcaseGlue in = { &cg_g.player_car, &cg_g.race_type, &cg_g.track, pointer_active_word(), &cg_g.buttons, car_flags, ctx, &step };
    glue_showcase(&in);
    return step;
}

static int central_up;  /* the race settings live in g while Control Central runs */

/* A window closed in the menus (port: the original quits only through them): the race settings into config.dat's
 * image as on leaving Control Central, while it runs */
void glue_settings_to_config(void)
{
    if (!central_up) return;
    const RaceSettings s = cg_settings_of();
    config_dat_set_race(config_current(), &s);
    cg_level_to_image();
}

int32_t glue_central(void)
{
    static const ControlCentralOps OPS = {
        pump, palette_clear, move_to, music, load_art, picture_new, sound_load, no_inverse, music_feed, mark, shape,
        cg_palette_set, cg_net_service, in_rect, car_flags, type_changed, ticks, chat_waiting, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, sound, deadline_set, cg_deadline_passed, cg_key, scancode, cheat_step,
        key_down, activate, net_start, click, kick, music_fade_out, fade_out, music_end, music_start, showcase,
        music_pick, step_race_type, track_forward, track_backward, nothing, cg_standings, track_select, art_free, options,
        cg_chat, glue_yes_no, glue_drain, glue_banner, pre_race, sound_free, picture_free, cg_flush, cg_race, off,
        cg_plus_text, NULL, 1, pad_buttons, palette_colour, card_marks, card_plain, cg_variants_chosen, cg_load_added_card, cg_load_track_card, cg_added_gallery, showcase_carousel };
    load();
    ControlCentralState st = {
        &cg_g.race_type, &cg_g.opponent, &cg_g.track, &cg_g.player_car, cg_g.other_car, &cg_g.net_car, &cg_g.laps, cg_g.difficulty,
        menu_words.mode_menu, cg_g.volumes, menu_words.link_game, link_view_only(), &menu_words.redraw, (int16_t)resource_german(), 0,
        &cg_g.tournament, &menu_words.link_lost, cg_g.races, menu_audio_kept(), (uint32_t *)&menu_cheats.keys_506,
        &cg_g.unlock_level, &cg_g.unlock_m, &cg_g.unlock_r, &cg_g.lost_vegas, &cg_g.vegas_cheat, &cg_g.net_player, &cg_g.net_player_was,
        cg_g.was, cg_g.art, &cg_g.variant_m, &cg_g.variant_r, menu_words.single, menu_words.intro_seen, &cg_g.nomercy, &cg_g.warrior,
        &cg_g.explode, menu_audio_silent(), &cg_g.inverse, &cg_g.nitro, &cg_g.net_hover, &cg_g.net_self, &cg_g.net_second, &cg_g.letter_held,
        link_humans(), link_names(), pointer_active_word(), &cg_g.buttons, ea_screen_slot(), "", keys, &warrior_rival_flag, &warrior_keep_flag, &central_edition };
    const LinkMenu link_menu = { &cg_g.race_type, &cg_g.track, &cg_g.laps, &cg_g.difficulty, &cg_g.net_car, &cg_g.nomercy, &cg_g.tournament,
                                 &cg_g.player_car, cg_g.other_car, &cg_g.variant_m, &cg_g.variant_r };
    link_central_bind(&link_menu);
    cg_extras_apply();   /* port: the switches are on from the first key, not after the first car change */
    picks_restore();
    tournament_leave_closed();
    central_up = 1, central_state = &st;
    cg_volumes_sync();
    const int32_t result = control_central(&st, &OPS);
    cg_volumes_sync();
    central_up = 0, central_state = NULL;
    link_central_bind(NULL);
    cg_extras_apply();   /* port: 00432ff4's start clears the cheats; the switches come back for the race */
    cg_picks_save();
    video_set_overlay(NULL, 0, 0);
    const RaceSettings s = cg_settings_of();
    config_dat_set_race(config_current(), &s);
    cg_level_to_image();
    return result;
}

/* --drive's race (port only, development): config.dat's settings with the track, its segment, the car and the race
 * type asked (-1 keeps config.dat's), run as Control Central's Race runs it: 00427ae0, so the race ends as the
 * original's does (the car slows to a stop, then the results screens) */
void glue_drive_race(int32_t track, int32_t segment, int32_t car, int32_t race_type, int32_t variant_m,
                     int32_t variant_r)
{
    glue_game_reset();  /* frontend_loop's 00466ae4 before Control Central: the names, the controls, the counts */
    const int16_t run = (int16_t)segment;  /* 00524e83, which 00427ae0 keeps as 0052b276: the run's file */
    memcpy(config_current()->image + SEGMENT_AT, &run, sizeof run);
    RaceSettings s = cg_settings_of();
    const int32_t kept_track = s.track;   /* an added track (16 on) is only for this race: config.dat keeps the track it had */
    if (track >= 0) s.track = track, s.variant = segment;  /* g.variant: the same short */
    s.variant_m = variant_m, s.variant_r = variant_r;  /* 004fbad8 / 004fbadc, as the cheats and track select set them */
    const char *const time = getenv("TNFS_DRIVE_TIME");  /* port: the track screen's time of day, 0..2 */
    if (time != NULL && track >= 0 && track < 9) cg_time_of_day[track] = atoi(time);
    if (car >= 0) s.player_car = car;
    if (race_type >= 0) s.race_type = race_type;
    cg_settings_put(&s);
    cg_race(NULL);
    RaceSettings after = cg_settings_of();
    if (track >= 16) after.track = kept_track;
    config_dat_set_race(config_current(), &after);
}

