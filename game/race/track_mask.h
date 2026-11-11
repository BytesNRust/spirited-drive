/* port: the hand-made reflective masks of a track's textures. Night, or a dark tunnel: what is reflective (road signs, posts, the cat's eyes painted on the asphalt of some roads)
 * shines when the car's headlights reach it. The owner paints a mask per texture, as for the cars: one PNG the size of the texture, <root>/<TRACK>/slot_<NN>.png for a road texture
 * slot (track_art.h: slot = id * 3 + letter, variant 0, the base size) and <root>/<TRACK>/scenery_<NN>.png for a scenery board (the signs and posts). White (red 128 or more) is
 * reflective; black and transparent are not. The mask can be any size (scaled by nearest neighbour to the texture's). A texture with no mask is not reflective. The masks hold no
 * colour and no pixel of the game. The files are looked up in $TNFS_TRACK_MASKS (a dev override: <dir of <TRACK>/<file>.png>), then <mod>/track_masks/, the program's data/track_masks/
 * and the source tree's (data_path.h). */
#ifndef TNFS_TRACK_MASK_H
#define TNFS_TRACK_MASK_H

#include "game/race/tunnel_lamp_textures.h"
#include "game/race/track_art.h"

#include <stddef.h>
#include <stdint.h>

/* Decodes a mask PNG into `out` (width x height bytes: 1 reflective, 0 not), scaled from the image's own size by nearest neighbour. A texel is reflective when its red is 128 or
 * more and it is not transparent (an indexed image's palette colour, opaque). Returns 0 for data that is not a readable PNG or a zero size. */
int track_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out);

/* The mask file `name` ("slot_07", "scenery_02") of `track` ("CL1") for a texture of width x height texels, from the first place that has it; malloc'd, width x height bytes; NULL
 * when there is none or it does not read. */
uint8_t *track_mask_load(const char *track, const char *name, int width, int height);

/* Loads the masks of `track`'s textures (each present slot's variant 0 and each scenery board of `art`) and keeps them until the next call; the previous track's are dropped.
 * `art` must stay as it is while they are used (the masks are found by the texture's address). Returns how many masks were found. NULL track or art: just drops them. */
int track_masks_set(const char *track, const TrackArt *art);

/* The same for a race: `rally` 1 (the track's rally variant: its road is gravel, dirt or snow) keeps no slot's mask (the cat's eyes painted on the asphalt are not there), only the scenery boards' (signs, posts).
 * track_masks_set is the circuit's (rally 0). */
int track_masks_set_race(const char *track, const TrackArt *art, int rally);

/* The mask of a texture of the track last given to track_masks_set (a slot's variant 0 as `&art->slots[s][0]`, or a scenery board `&art->scenery[i]`): bitmap.width x bitmap.height
 * bytes, 1 reflective, 0 not; NULL when it has no mask (or `t` is NULL). */
const uint8_t *track_mask_for(const TrackTexture *t);

/* port (lamp marks): a lamp is YELLOW in a road texture's mask (slot_<NN>.png): red and green 128 or more, blue below 128, opaque. The lamp's texture is the slot, its place in the texture the bounding
 * box of the yellow texels, its colours the palette entries those texels show (the most used first, at most TUNNEL_LAMP_TEXTURE_COLOURS). A yellow texel is not reflective (red stays so, and white). Yellow in a scenery
 * board's mask (scenery_<NN>.png) is read as not reflective and used for nothing yet. The marks are read by track_masks_set_race, rally or not. */

/* The lamps the yellow marks of the track last given to track_masks_set_race name (`track` must be that one, in any case): how many, `*out` (when not NULL) them; 0 when the track has none. */
int track_lamp_marks(const char *track, const TunnelLampTexture **out);

/* The marked lamps' colours read again from their textures' texels as they are now (the yellow kept from track_masks_set_race): the race's palette remaps the track's texels in place
 * after the marks were read (the exe's mapping to the screen palette), and the colours read before named entries the lamp no longer shows. Nothing kept: nothing done. */
void track_lamp_marks_refresh(void);

/* The lamps of `track`: the marks' when it has any (they are the owner's drawing), else the built-in table's (tunnel_lamp_textures_for: unpainted tracks and mods keep what they had). */
int track_lamps_for(const char *track, const TunnelLampTexture **out);

#endif
