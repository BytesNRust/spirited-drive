/* port (the PS1 look's light, psx_light.h): the tunnels' lamps. The baked light darkens a tunnel's walls and ceiling, and with them the
 * texture of each ceiling lamp; the PS1 keeps the lamp itself bright. A lamp is a texture of the track (a slot of the track's TRKD
 * texture bytes, id * 3 + 0..2) and, inside it, a few palette entries (the fitting's colours): in that texture only, those entries
 * are not darkened. The palette entries are the PC's and are shared with the sky and the windows, so the texture is the key, never the
 * entry alone. Found by tnfs-decomp's helper from the PC tracks' textures; no PS1 data is used. */
#ifndef TNFS_PSX_LAMPS_H
#define TNFS_PSX_LAMPS_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_LAMP_COLOURS = 8 };

typedef struct {
    int slot;                        /* the track's texture byte: slot of TrackArt, in every variant it has */
    int count;                       /* the colours below */
    uint8_t idx[PSX_LAMP_COLOURS];   /* palette entries of the lamp's fitting */
    int rect[4];                     /* x0, y0, x1, y1 (inclusive, texels): where the fitting and its glow are; the colours seen only there stay bright too */
} PsxLamp;

/* The lamps of the track (CY1, CY2, CY3, TR4, in any case; the others have none): how many, and `*out` (when not NULL) the table. */
int psx_lamps_for(const char *track, const PsxLamp **out);

/* The palette entries of a lamp that stay bright: its listed colours, and the colours its texture (w x h palette indices) shows inside its
 * rectangle and nowhere outside it (the glow's greys, which the dark wall of the same texture does not use). `exempt`: 256 flags. */
void psx_lamp_exempt(const PsxLamp *lamp, const uint8_t *pixels, int w, int h, uint8_t exempt[256]);

/* `out`: `levels` darkening tables like `lit` (256 bytes each, level 0 the world's lightest) with the `exempt` colours mapped to themselves. */
void psx_lamp_tables(const uint8_t exempt[256], const uint8_t (*lit)[256], int levels, uint8_t (*out)[256]);

/* port (night): the texels of a lamp texture (`count` palette indices) with each colour `remap` names changed to its entry (remap[c] != c); the night's
 * lamps are drawn in entries of their own that the tint leaves alone. Returns how many texels changed. */
size_t psx_lamp_recolour(uint8_t *pixels, size_t count, const uint8_t remap[256]);

/* port (night): the same, but only inside the lamp's own rectangle of a w x h texture: the rest of the texture (walls, signs, neon sharing the
 * lamp's colours) keeps its entries and is tinted with the night. */
size_t psx_lamp_recolour_rect(const PsxLamp *lamp, uint8_t *pixels, int w, int h, const uint8_t remap[256]);

#endif
