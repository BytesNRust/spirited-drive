/* port: the PS1 second livery of a car (extras.cfg psx_liveries). The PS1 CD has two body liveries for each of the
 * eight drivable cars, Z<CAR>.VSM (the colour the PC has) and Z<CAR>.WSM (the alternate). Their textures are repainted
 * copies of one another, so the alternate is taken as a colour map: the body colours the normal livery uses that the
 * alternate does not, matched by luminance rank onto the alternate's own, weight for weight. The map is applied to a
 * rival's CFM palette when it loads (drive_livery.c), so only that car is repainted. Nothing here is EA data: the
 * tables are built from the user's own files, and the tests use synthetic colours. */
#ifndef TNFS_CAR_LIVERY_H
#define TNFS_CAR_LIVERY_H

#include "formats/psx_bigf.h"

#include <stdint.h>

enum { CAR_LIVERY_PAIRS = 1024 };   /* the most body colours a map takes; a car with more gets no alternate */

/* one body colour: `from` is the normal livery's colour, `to` the alternate's, both BGR555 (5 bits a channel) */
typedef struct {
    uint16_t from, to;
} CarLiveryPair;

/* The map from the normal livery's colour counts (`normal`) to the alternate's (`alternate`), both PSX_COLOURS long.
 * Only the colours one livery has and the other lacks are mapped, plus the `forced` colours (BGR555): the body colours
 * the PC palette has that the PS1 textures share with the alternate (a grey trim both liveries paint), which the count
 * alone would leave out. A forced colour weighs as the mean body colour. Writes up to CAR_LIVERY_PAIRS pairs into
 * `pairs` and returns their number; 0 when either livery has no body colours to map, or more than CAR_LIVERY_PAIRS. */
int car_livery_map(const uint32_t *normal, const uint32_t *alternate, const uint16_t *forced, int forced_count,
                   CarLiveryPair pairs[CAR_LIVERY_PAIRS]);

/* The 5-bit colour (BGR555) of a palette entry of this type: 0x24 as the PC wrote it (8 bits a channel), 0x22 6 bits. */
uint16_t car_livery_colour(const uint8_t *entry, int type);

/* Remaps the 256 colours of a CFM palette in place. `type` is the shape's (0x24: 8 bits a channel, as the PC wrote it;
 * 0x22: 6 bits a channel); any other type is left alone. A colour whose 5-bit form is in `pairs` is replaced by the
 * pair's alternate, the rest are kept. */
void car_livery_remap(uint8_t *palette, int type, const CarLiveryPair *pairs, int count);

/* 1 when a rival takes the alternate livery, 0 the normal one. The rival is `slot` racing `car` (its CFM index) with
 * the player in `player_car` (-1 for none) wearing `player_livery`. A rival in the player's car always wears the
 * other livery; any other rival's choice is a hash of the race's seed and its slot: the same race replays with the
 * same colours, and the game's own random stream is not drawn from. */
int car_livery_pick(uint32_t seed, int slot, int car, int player_car, int player_livery);

#endif
