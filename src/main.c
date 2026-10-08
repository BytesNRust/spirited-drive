/* Entry point.
 *
 *   tnfs [--data DIR] [--german]          run the game (00437540)
 *   tnfs --view FILE [--entry N] [--screenshot OUT.ppm]
 *                                          browse an FSH / QFS (development) */
#include "game/frontend/frontend.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "platform/platform.h"
#include "viewer/asset_viewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *data_root;
    int german;
    const char *view;
    const char *screenshot;
    uint32_t entry;
} Options;

static int parse_args(int argc, char **argv, Options *opt)
{
    *opt = (Options){ "original", 0, NULL, NULL, 0 };
    for (int i = 1; i < argc; i++) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--data") == 0 && has_value) {
            opt->data_root = argv[++i];
        } else if (strcmp(argv[i], "--german") == 0) {
            opt->german = 1;
        } else if (strcmp(argv[i], "--view") == 0 && has_value) {
            opt->view = argv[++i];
        } else if (strcmp(argv[i], "--screenshot") == 0 && has_value) {
            opt->screenshot = argv[++i];
        } else if (strcmp(argv[i], "--entry") == 0 && has_value) {
            opt->entry = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "usage: %s [--data DIR] [--german] | --view FILE [--entry N] [--screenshot OUT.ppm]\n",
                    argv[0]);
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
    frontend_run();
    pointer_free();
    video_shutdown();
    platform_shutdown();
    return 1;
}

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt)) return EXIT_FAILURE;
    const int ok = opt.view != NULL ? asset_viewer_main(opt.view, opt.entry, opt.screenshot) : run_game(&opt);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
