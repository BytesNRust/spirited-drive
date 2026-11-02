/* port (night, psx_headlights): the lit windows of the night's buildings. A building is a scenery board (a sprite of a facade) whose windows lie on a grid;
 * at night a seeded share of them is painted in a warm entry of its own (the lamp pool's, which the light tables leave alone). The board is one texture for
 * every building drawn from it, so the same windows are lit on all its copies. No PS1 data is used. */
#ifndef TNFS_WINDOW_LIGHTS_H
#define TNFS_WINDOW_LIGHTS_H

#include <stddef.h>
#include <stdint.h>

enum { WINDOW_COLS_MAX = 12, WINDOW_ROWS_MAX = 20 };

typedef struct {
    int scenery;                          /* the board: TrackArt.scenery[scenery] */
    int width, height;                    /* its size, to be sure it is the one */
    int ncol, nrow;                       /* the windows' grid: ncol columns by nrow rows */
    int col[WINDOW_COLS_MAX][2];          /* each column's x0, x1 inclusive */
    int row[WINDOW_ROWS_MAX][2];          /* each row's y0, y1 inclusive */
} WindowBoard;

/* The track's window boards (any case): their count, `*out` pointing at them; 0 for a track with none. */
int window_boards_for(const char *track, const WindowBoard **out);

/* Paints a seeded share (two in five) of the board's windows in `entry` over their whole rectangle; returns the windows lit. Painting again changes nothing;
 * a board of another size is left alone (0). */
size_t window_board_paint(const WindowBoard *board, uint8_t *pixels, int width, int height, uint8_t entry, uint32_t seed);

/* A board whose windows are not on a grid (a facade with its own openings): the windows are its dark patches. A patch is a 4-connected run of texels darker than
 * `dark_luma` (0..255, from the board's palette) inside `region`, as big as a window (min..max) and not the transparent index. A seeded share of the patches is
 * painted, texel by texel, so the frame round the glass stays as it is. */
typedef struct {
    int scenery;
    int width, height;
    int region[4];                        /* x0, y0, x1, y1 inclusive: where to look */
    int dark_luma;
    int min_w, min_h, max_w, max_h;       /* a patch's bounding box */
    uint8_t transparent;
} WindowDark;

/* The track's by-darkness boards (any case): their count, `*out` pointing at them; 0 for a track with none. */
int window_darks_for(const char *track, const WindowDark **out);

/* Paints a seeded share (two in five) of the board's dark patches in `entry` (`palette` is 3 bytes per index); returns the patches lit. Painting again changes
 * nothing; a board of another size is left alone (0). */
size_t window_dark_paint(const WindowDark *board, uint8_t *pixels, int width, int height, const uint8_t *palette, uint8_t entry, uint32_t seed);

#endif
