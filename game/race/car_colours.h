/* port (car colours): the screen palette's slots the cars own, filled with the colours the cars show most. The exe queues a car's
 * own '!xxx' colours (23 for the player, 8 a rival out of a race) or the shared car palette (72, in a race) onto fixed screen entries
 * and maps every texel to the nearest colour of the whole screen palette; the track fills the rest, so a body's light, softly
 * coloured tones (a teal highlight) have no neighbour and land on a blue grey. Here the slots are chosen after the race palette is
 * written and before the cars are mapped: of the colours the cars' textures use, the ones that most reduce the distance of every texel to
 * its nearest screen colour (a greedy cover), and a block whose colours already do as well is left alone. Off (0) is the exe. */
#ifndef TNFS_CAR_COLOURS_H
#define TNFS_CAR_COLOURS_H

#include "game/race/race_palette.h"

typedef struct {
    int first, count;
    int car;                  /* the slot the block serves, -1 every car */
    int colours;              /* the distinct colours the textures it serves use */
    int changed;              /* the block's entries given a new colour */
    double before, after;     /* the mean distance (8-bit RGB, weighted) of a texel to its nearest screen colour */
} CarFitBlock;

typedef struct {
    int count;
    CarFitBlock block[RACE_PALETTE_BLOCKS];
} CarFitReport;

/* Fits the blocks of the race palette `p` (already applied: race_palette_apply) to the cars in `art` (their banks as the loaders left them,
 * not yet mapped). `have_carpal`: the race has the shared car palette. Writes the new colours to the screen palette; `report` (may be NULL)
 * tells what each block did. Returns the entries changed. */
int car_colours_fit(RacePalette *p, const RaceState *race, const RaceCarArt art[RACE_CARS], int have_carpal, CarFitReport *report);

#endif
