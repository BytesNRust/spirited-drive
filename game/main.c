/* Entry point.
 *
 *   tnfs [--data DIR] [--mods DIR] [--german] [--net-port N] [--net-host IP:PORT]...
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
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "game/sys/port_icon.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/race/race_sim.h"
#include "platform/platform.h"
#include "viewer/asset_viewer.h"
#include "viewer/drive_joy.h"
#include "viewer/track_viewer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *data_root;
    const char *mods;
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

static int parse_args(int argc, char **argv, Options *opt)
{
    *opt = (Options){ "original", NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, TRACK_VIEWER_RACERS, -1, NULL, 0, 0, { NULL },
                      0 };
    for (int i = 1; i < argc; i++) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--data") == 0 && has_value) {
            opt->data_root = argv[++i];
        } else if (strcmp(argv[i], "--mods") == 0 && has_value) {
            opt->mods = argv[++i];
        } else if (strcmp(argv[i], "--german") == 0) {
            opt->german = 1;
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
            fprintf(stderr, "usage: %s [--data DIR] [--mods DIR] [--german] [--net-port N] [--net-host IP:PORT]... | --view FILE [--entry N] [--screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --track NAME [--node N] [--screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --drive NAME[m|r|mr] [--car N] [--racers 1..8 | --type 0..3] [--node TICKS --screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --replay FILE.RPL\n"
                            "       %s [--data DIR] --movie FILE.TGV\n",
                    argv[0], argv[0], argv[0], argv[0], argv[0]);
            return 0;
        }
    }
    return 1;
}

/* --movie: the window, the menus' sound and one movie (development) */
static int run_movie(const Options *opt)
{
    const PlatformConfig config = port_platform_config("The Need for Speed SE");
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: the CD's NFSICONN.ICO on the window */
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
        fprintf(stderr, "--drive: no track %s\n", opt->drive);
        return 0;
    }
    const PlatformConfig config = port_platform_config("The Need for Speed SE");
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: the CD's NFSICONN.ICO on the window */
    resource_set_units(port_extras_load().units);   /* port: the speed's units from extras.cfg, the language's by default */
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
    const PlatformConfig config = port_platform_config("The Need for Speed SE");
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    port_icon_apply();   /* port: the CD's NFSICONN.ICO on the window */
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
    frontend_run();
    text_free();
    pointer_free();
    video_shutdown();
    platform_shutdown();
    return 1;
}

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt)) return EXIT_FAILURE;
    int ok;
    if (opt.mods != NULL && !platform_set_mods(opt.data_root, opt.mods)) return EXIT_FAILURE;
    if (opt.view != NULL) ok = asset_viewer_main(opt.view, opt.entry, opt.screenshot);
    else if (opt.replay != NULL) ok = track_viewer_drive_replay(opt.data_root, opt.replay);
    else if (opt.movie != NULL) ok = run_movie(&opt);
    else if (opt.drive != NULL && opt.screenshot == NULL && opt.entry == 0) ok = run_drive(&opt);
    else if (opt.drive != NULL) {
        /* port: the headless frame takes the language and the speed's units as the race does (run_drive's setup) */
        resource_init(opt.data_root, opt.german);
        resource_set_units(port_extras_load().units);
        ok = track_viewer_drive(opt.data_root, opt.drive, opt.car, opt.racers, opt.race_type, opt.entry, opt.screenshot);
    }
    else if (opt.track != NULL) ok = track_viewer_main(opt.data_root, opt.track, opt.entry, opt.screenshot);
    else ok = run_game(&opt);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
