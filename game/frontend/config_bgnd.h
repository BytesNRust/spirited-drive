/* port: the configuration screens' background without its burnt-in text, for the port's own screens (Graphics,
 * EXTRAS). The CD holds no clean copy: every Options screen ships the same picture with its own title and labels
 * burnt in. The clean one is rebuilt from the CD at run time, so no EA pixels live in the repository: per pixel the
 * medoid of the ten screens that share the picture, the two bands where most of them carry text taken from a screen
 * that has none there, the rest of the title covered with the texture beside it, then the nearest colours of
 * OPTIONS.QFS's palette. Same CD files, same pixels. */
#ifndef TNFS_CONFIG_BGND_H
#define TNFS_CONFIG_BGND_H

#include <stddef.h>
#include <stdint.h>

enum {
    CONFIG_BGND_W = 640,
    CONFIG_BGND_H = 480,
    CONFIG_BGND_SCREENS = 10,  /* OPTION/: OPTIONS AUDIO GRAPHICS CNTRLSET LOADSAVE MODCONF MODEMSET NETSET, CONTROL/: GAMEMODE GAMEMOD2 */
    CONFIG_BGND_OPTIONS = 0,   /* the screens whose palette a caller may ask for */
    CONFIG_BGND_GAME_MODE = 8
};

/* The ten screens' "bgnd" as RGB (W x H x 3 each, in the order above) into palette indices of `palette`. With
 * `done_cancel` 0 the bottom corners' DONE and CANCEL are left out (taken from Game Mode, which has neither). */
void config_bgnd_compose(const uint8_t *const rgb[CONFIG_BGND_SCREENS], const uint8_t palette[768], int done_cancel,
                         uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H]);

/* One art file by its name under the art directory ("option/audio.qfs"), decompressed, freed with free(); NULL when
 * it cannot be read. The game passes resource_load_art. */
typedef uint8_t *(*ConfigBgndRead)(const char *name, size_t *size);

/* Reads the ten screens and composes them in the palette of screen `palette_screen` (CONFIG_BGND_OPTIONS, or
 * CONFIG_BGND_GAME_MODE to patch Game Mode's own picture), returned in `palette`. 1 on success, 0 when a screen is
 * missing or not 640x480. */
int config_bgnd_load(ConfigBgndRead read, int palette_screen, int done_cancel,
                     uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768]);

#endif
