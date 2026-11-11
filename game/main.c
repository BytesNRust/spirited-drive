/* Entry point.
 *
 *   tnfs [--data DIR] [--mods DIR] [--user DIR] [--german] [--verbose] [--net-port N] [--net-host IP:PORT]...
 *                                          run the game (00437540); files of the mod in DIR replace the CD's; the
 *                                          network game binds UDP port N (15000 by default) and also asks each
 *                                          --net-host (up to 8) for games besides the LAN's broadcast
 *   tnfs --view FILE [--entry N] [--screenshot OUT.ppm]
 *                                          browse an FSH / QFS (development)
 *   tnfs --track NAME [--node N] [--screenshot OUT.ppm]
 *                                          fly along a track (development)
 *   tnfs --drive NAME [--car N] [--type 0..3]   a race as the menus run it, ending on the results screens; NAME
 *                                          ending in m or r (TR1m, CY2r) takes the track's "m" or "r" art and layout
 *   tnfs --drive NAME [--car N] [--racers N] --node TICKS --screenshot OUT.ppm   a headless frame
 *   tnfs --replay FILE.RPL                 play a replay back in the drive (development)
 *   tnfs --movie FILE.TGV                  play a TGV movie in the menus' window (development) */
#include "game/frontend/config.h"
#include "game/frontend/frontend.h"
#include "game/frontend/link_game.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_glue.h"
#include "game/frontend/tgv_player.h"
#include "game/platform/first_run.h"
#include "game/platform/update.h"
#include "game/sys/art_cache.h"
#include "game/sys/data_path.h"
#include "game/sys/data_root.h"
#include "game/sys/game_name.h"
#include "game/exe/exe_tables.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_old_files.h"
#include "game/sys/port_settings.h"
#include "game/sys/port_stats.h"
#include "game/sys/port_icon.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/race/race_sim.h"
#include "platform/platform.h"
#include "tnfs_version.h"
#include "viewer/asset_viewer.h"
#include "viewer/drive_joy.h"
#include "viewer/track_viewer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *data_root;
    int data_explicit;   /* --data was given */
    const char *mods;
    const char *user;   /* --user DIR: the player's folder */
    int german;
    const char *view;
    const char *track;
    const char *drive;
    const char *replay;
    const char *movie;
    int car;
    int racers;
    int race_type;
    const char *screenshot;
    uint32_t entry;
    int net_port;
    const char *net_hosts[NET_UDP_HOSTS + 1];  /* NULL-terminated */
    int net_host_count;
} Options;

/* the usage message, as a fatal; returns 0 for a caller that goes on */
static int bad_usage(const char *program)
{
    port_fatal("usage: %s [--data DIR] [--mods DIR] [--user DIR] [--german] [--verbose] [--net-port N] [--net-host IP:PORT]... | --view FILE [--entry N] [--screenshot OUT.ppm]\n"
                    "       %s [--data DIR] --track NAME [--node N] [--screenshot OUT.ppm]\n"
                    "       %s [--data DIR] --drive NAME[m|r|mr] [--car N] [--racers 1..8 | --type 0..3] [--node TICKS --screenshot OUT.ppm]\n"
                    "       %s [--data DIR] --replay FILE.RPL\n"
                    "       %s [--data DIR] --movie FILE.TGV\n",
            program, program, program, program, program);
    return 0;
}

static int parse_args(int argc, char **argv, Options *opt)
{
    *opt = (Options){ "original", 0, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, TRACK_VIEWER_RACERS, -1, NULL, 0, 0, { NULL },
                      0 };
    for (int i = 1; i < argc; i++) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--data") == 0 && has_value) {
            opt->data_root = argv[++i];
            opt->data_explicit = 1;
        } else if (strcmp(argv[i], "--mods") == 0 && has_value) {
            opt->mods = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 && has_value) {
            opt->user = argv[++i];
        } else if (strcmp(argv[i], "--german") == 0) {
            opt->german = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            port_log_set_verbose(1);
        } else if (strcmp(argv[i], "--view") == 0 && has_value) {
            opt->view = argv[++i];
        } else if (strcmp(argv[i], "--track") == 0 && has_value) {
            opt->track = argv[++i];
        } else if (strcmp(argv[i], "--drive") == 0 && has_value) {
            opt->drive = argv[++i];
        } else if (strcmp(argv[i], "--replay") == 0 && has_value) {
            opt->replay = argv[++i];
        } else if (strcmp(argv[i], "--movie") == 0 && has_value) {
            opt->movie = argv[++i];
        } else if (strcmp(argv[i], "--racers") == 0 && has_value) {
            opt->racers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--type") == 0 && has_value) {
            opt->race_type = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--car") == 0 && has_value) {
            opt->car = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--entry") == 0 || strcmp(argv[i], "--node") == 0) && has_value) {
            opt->entry = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--screenshot") == 0 && has_value) {
            opt->screenshot = argv[++i];
        } else if (strcmp(argv[i], "--net-port") == 0 && has_value && atoi(argv[i + 1]) > 0 && atoi(argv[i + 1]) < 65536) {
            opt->net_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--net-host") == 0 && has_value && opt->net_host_count < NET_UDP_HOSTS) {
            opt->net_hosts[opt->net_host_count++] = argv[++i];
        } else {
            return bad_usage(argv[0]);
        }
    }
    return 1;
}

/* --movie: the window, the menus' sound and one movie (development) */
static int run_movie(const Options *opt)
{
    const PlatformConfig config = port_platform_config(GAME_NAME);
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: our icon on the window (the CD's NFSICONN.ICO without it) */
    timer_init();
    menu_audio_open();
    printf("movie %s: %s\n", opt->movie, tgv_player_play(opt->movie) ? "stopped" : "ran to its end");
    menu_audio_close();
    video_shutdown();
    platform_shutdown();
    return 1;
}

/* 004322f8's 0043c744: the joystick before config.dat is read (0042a010 writes 0052ad26 over what it leaves
 * there, so it works on a copy here) */
static void joy_start_up(int german)
{
    int16_t controls[JOY_CONTROLS] = { 0 };
    drive_joy_start(controls, (int16_t)german);
    input_on_keys(menu_audio_sound_toggle, drive_joy_menu);
}

/* --drive without --node / --screenshot: the race as the menus run it (glue_drive_race), so it ends as the
 * original's does, the car slowing to a stop and then the results screens; the track's name picks the track and
 * its variant, --car the car, --type the race type (2, the full field, when not given).
 *
 * port: the name's trailing "m"/"r" letters may be one or both ("TR1mr"), so the Mirror and Rally can be raced
 * together on one run — the state only the port reaches, since the original's screen and cheats never have both on.
 * Only the tail counts: "TR1"'s own R is part of the track's name, not a request for the rally. */
static void drive_variants(const char *name, int *variant_m, int *variant_r)
{
    size_t n = strlen(name);
    while (n > 0) {
        const char c = (char)tolower((unsigned char)name[n - 1]);
        if (c == 'm') *variant_m = 1, n--;
        else if (c == 'r') *variant_r = 1, n--;
        else break;
    }
}

static int run_drive(const Options *opt)
{
    enum { FULL_FIELD = 2 };
    const int track = race_sim_track_index(opt->drive);
    if (track < 0) {
        port_fatal("--drive: no track %s\n", opt->drive);
        return 0;
    }
    const PlatformConfig config = port_platform_config(GAME_NAME);
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: our icon on the window (the CD's NFSICONN.ICO without it) */
    resource_set_units(port_extras_load().units);   /* port: the speed's units from the mode's file, the language's by default */
    timer_init();
    pointer_load(0);
    text_init();
    pointer_start();
    pointer_on_wait(menu_audio_music_service);
    joy_start_up(opt->german);
    menu_audio_open();
    config_load();
    int variant_m = 0, variant_r = 0;
    drive_variants(opt->drive, &variant_m, &variant_r);
    glue_drive_race(track, race_sim_track_variant(opt->drive), opt->car,
                    opt->race_type >= 0 ? opt->race_type : FULL_FIELD, variant_m, variant_r);
    menu_audio_close();
    text_free();
    pointer_free();
    video_shutdown();
    platform_shutdown();
    return 1;
}

/* 00437540: the game's main(). */
static int run_game(const Options *opt)
{
    const PlatformConfig config = port_platform_config(GAME_NAME);
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: our icon on the window (the CD's NFSICONN.ICO without it) */
    /* port: the speed's units from the mode's file from the first screen (run_drive and the headless frame set them; the menus' game never did, so km/h in the file
     * read mph until CUSTOMIZE's Done was pressed in the session) */
    resource_set_units(port_extras_load().units);
    port_log("units: %s\n", resource_kmh() ? "km/h" : "mph");
    timer_init();
    config_dat_startup_volume(config_current());
    /* 004322f8 loads the pointer; port: its CD and memory checks have nothing to check here, the fonts are
     * text_init's and config.dat is the menus' (0042a010, config_load). The network: the link game over UDP
     * (link_game.c) */
    link_game_init((uint16_t)opt->net_port, opt->net_hosts);
    pointer_load(0);
    text_init();
    pointer_start();
    pointer_on_wait(menu_audio_music_service);
    joy_start_up(opt->german);
    if (port_extras_load().update_check) platform_update_check_start(TNFS_VERSION);   /* port: one background question to our API (it counts the start), never blocking */
    port_stats_event(STATS_FIRST_START);   /* port: once per install, only with the same switch On; a dev build sends nothing */
    frontend_run();
    text_free();
    pointer_free();
    video_shutdown();
    platform_shutdown();
    return 1;
}

/* port (the first-run wizard): --data as given, else the player's own copy of the CD (<user dir>/gamedata), else a development build's original/, else the wizard asks for the
 * CD's folder and makes the copy */
static int resolve_data_root(const Options *opt, char *out, size_t out_size, int *data_copied)
{
    char copy[1024];
    const int has_copy = first_run_user_copy(copy, sizeof copy);
#ifdef TNFS_SOURCE_DIR
    const char *const dev = "original";   /* a development build, run from the repository */
#else
    const char *const dev = NULL;
#endif
    if (data_root_resolve(opt->data_explicit ? opt->data_root : NULL, has_copy ? copy : NULL, dev, out, out_size) != DATA_ROOT_NEEDS_SETUP) return 1;
    if (!has_copy || !first_run_setup(copy)) {
        port_fatal("no game data: run it once without --data on a machine with a display and choose the CD's folder, or give --data DIR\n");
        return 0;
    }
    *data_copied = 1;   /* port: counted as data_ready once the mods directory is in place (below) */
    snprintf(out, out_size, "%s", copy);
    return 1;
}

/* port: the events go to our API; a development build (TNFS_SOURCE_DIR, never a release) with TNFS_STATS_DRYRUN=<version> counts them as that version and
 * only writes what it would send to the log, so the tests can run a "release" without a request */
#ifdef TNFS_SOURCE_DIR
static void stats_dry_sender(const char *event, const char *version) { port_log("stats: dry run, would send %s v=%s\n", event, version); }
#endif

static void set_stats_sender(void)
{
#ifdef TNFS_SOURCE_DIR
    const char *const dry = getenv("TNFS_STATS_DRYRUN");
    if (dry != NULL && dry[0] != '\0') {
        port_stats_set_version(dry);
        port_stats_set_sender(stats_dry_sender);
        return;
    }
#endif
    port_stats_set_sender(platform_stats_post);
}

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt)) return EXIT_FAILURE;
    port_settings_set_display_probe(platform_desktop_size);   /* the profiles' widescreen is the display's own shape */
    set_stats_sender();
    platform_set_user_dir(opt.user);   /* port: before the log opens in it */
    platform_open_log(opt.mods);   /* before anything can stop the run: the report of a run with no game data is in spirited-drive.log too */
    {   /* port: the pictures made from the CD's art are kept in <user folder>/cache/ (art_cache.h) */
        char cache[1100];
        if (platform_user_dir(cache, sizeof cache - 8)) art_cache_set_dir(strcat(cache, "cache/"));
    }
    int ok;
    static char data_root[1024];
    int data_copied = 0;
    if (!resolve_data_root(&opt, data_root, sizeof data_root, &data_copied)) return EXIT_FAILURE;
    opt.data_root = data_root;
    {   /* port: the tables of the original's exe, from the player's own copy */
        char tables_error[1200];
        if (exe_tables_install(opt.data_root, tables_error, sizeof tables_error) != 0) {
            port_fatal("%s\n", tables_error);
            return EXIT_FAILURE;
        }
    }
    if (!platform_set_mods(opt.data_root, opt.mods)) return EXIT_FAILURE;   /* port: --mods DIR, else <user folder>/mods/ when it exists */
    data_path_set_mod(platform_mods_dir());   /* port: a mod's own copy of a data file (track_light/, car_masks/, dash_masks/, sounds/, car_envmap.png) comes before the port's data/ */
    port_old_files_migrate();   /* before any setting is read, and before there are threads */
    if (data_copied) port_stats_event(STATS_DATA_READY);   /* port: the CD's data copied and ready (once per install, the switch On); stats.cfg goes to the player's folder (--user DIR) */
    if (opt.view != NULL) ok = asset_viewer_main(opt.view, opt.entry, opt.screenshot);
    else if (opt.replay != NULL) ok = track_viewer_drive_replay(opt.data_root, opt.replay);
    else if (opt.movie != NULL) ok = run_movie(&opt);
    else if (opt.drive != NULL && opt.screenshot == NULL && opt.entry == 0) ok = run_drive(&opt);
    else if (opt.drive != NULL) {
        /* port: the headless frame takes the language and the speed's units as the race does (run_drive's setup) */
        resource_init(opt.data_root, opt.german);
        resource_set_units(port_extras_load().units);
        ok = track_viewer_drive(opt.data_root, opt.drive, opt.car, opt.racers, opt.race_type, opt.entry, opt.screenshot);
        /* port (dev, env only): TNFS_DRIVE_THEN=NAME races a second track in the same process, as the menus do after a race (the tests for what a track leaves behind) */
        const char *then = getenv("TNFS_DRIVE_THEN");
        if (ok && then != NULL && then[0] != '\0') ok = track_viewer_drive(opt.data_root, then, opt.car, opt.racers, opt.race_type, opt.entry, opt.screenshot);
    }
    else if (opt.track != NULL) ok = track_viewer_main(opt.data_root, opt.track, opt.entry, opt.screenshot);
    else ok = run_game(&opt);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
