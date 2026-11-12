/* port: Spirited Drive's own icon for the window, data/icon/spirited_<side>.png (art/README.md), read through the game's data search path (data_path.h) so
 * a mod's copy comes first. The sizes go to the window together: the 32 px picture (the key, drawn for small sizes) is the one at 100% scale and the bigger
 * ones are its high-DPI versions, so the platform picks the picture made for each size. */
#ifndef TNFS_PORT_ICON_OURS_H
#define TNFS_PORT_ICON_OURS_H

#include <stdint.h>

enum { PORT_ICON_SIZES = 5 };   /* 32, 48, 64, 128, 256 */

typedef struct {
    int side;        /* the PNG's width and height */
    uint8_t *rgba;   /* side x side RGBA, top row first; free with port_icon_ours_free */
} PortIconImage;

/* The sizes found, smallest first, into `out`; how many (0: none, the caller falls back to the CD's icon). A file that is not a square PNG of its name's size
 * is passed over. Each size is looked for as data_path_candidates_for(mod, base, source_dir, "icon/spirited_<side>.png") gives it. */
int port_icon_ours_from(const char *mod, const char *base, const char *source_dir, PortIconImage out[PORT_ICON_SIZES]);

/* The same with the mod, base and source tree the game set (data_path_candidates). */
int port_icon_ours(PortIconImage out[PORT_ICON_SIZES]);

void port_icon_ours_free(PortIconImage *images, int count);

/* Whether the game sets a window icon at all: not on macOS, where it would replace the Dock icon while the game runs and these full-bleed pictures would show
 * bigger than other apps'; there the bundle's .icns, drawn on Apple's icon grid, gives the Dock icon. */
int port_icon_sets_window(void);

#endif
