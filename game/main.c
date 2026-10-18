/* Entry point.
 *
 *   tnfs [--data DIR] [--mods DIR] [--german]  run the game (00437540); files of the mod in DIR replace the CD's
 *   tnfs --view FILE [--entry N] [--screenshot OUT.ppm]
 *                                          browse an FSH / QFS (development)
 *   tnfs --track NAME [--node N] [--screenshot OUT.ppm]
 *                                          fly along a track (development)
 *   tnfs --drive NAME [--car N] [--racers N] [--node TICKS --screenshot OUT.ppm]
 *                                          drive a car on a track (development)
 *   tnfs --replay FILE.RPL                 play a replay back in the drive (development) */
#include "game/frontend/frontend.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "platform/platform.h"
#include "viewer/asset_viewer.h"
#include "viewer/track_viewer.h"

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
    int car;
    int racers;
    int race_type;
    const char *screenshot;
    uint32_t entry;
} Options;

static int parse_args(int argc, char **argv, Options *opt)
{
    *opt = (Options){ "original", NULL, 0, NULL, NULL, NULL, NULL, 0, TRACK_VIEWER_RACERS, -1, NULL, 0 };
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
        } else {
            fprintf(stderr, "usage: %s [--data DIR] [--mods DIR] [--german] | --view FILE [--entry N] [--screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --track NAME [--node N] [--screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --drive NAME [--car N] [--racers 1..8 | --type 0..3] [--node TICKS --screenshot OUT.ppm]\n"
                            "       %s [--data DIR] --replay FILE.RPL\n",
                    argv[0], argv[0], argv[0], argv[0]);
            return 0;
        }
    }
    return 1;
}

/* 00437540: the game's main(). */
static int run_game(const Options *opt)
{
    const PlatformConfig config = { VIDEO_WIDTH, VIDEO_HEIGHT, "The Need for Speed SE" };
    if (!platform_init(&config)) return 0;
    if (!video_init()) {
        platform_shutdown();
        return 0;
    }
    resource_init(opt->data_root, opt->german);
    timer_init();
    /* 004322f8 loads the pointer; TODO(004322f8): CD/memory checks, fonts,
     * config; TODO(004100ec): network. */
    pointer_load(0);
    text_init();
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
    else if (opt.drive != NULL) ok = track_viewer_drive(opt.data_root, opt.drive, opt.car, opt.racers, opt.race_type, opt.entry, opt.screenshot);
    else if (opt.track != NULL) ok = track_viewer_main(opt.data_root, opt.track, opt.entry, opt.screenshot);
    else ok = run_game(&opt);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
