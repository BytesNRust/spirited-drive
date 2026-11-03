/* port (extras.cfg's rally_gravel): the rally's loose road made to feel like one. In the exe the rally only makes every surface loose
 * (race_surfaces_init): its tyres throw dirt and mark the road only when they slide, and its gravel sound is a track 5 off-road
 * oddity. With the switch the rally rolls on a gravel noise all along, and by what the track is made of:
 *   compacted gravel (City, Coastal, Alpine): the exe's dirt, when the tyres slide, nothing more
 *   dust (the desert, Rusty Springs): the rear tyres leave a low dust trail all the time they roll (dust_trail.h)
 *   regolith (Lunar Springs, the moon's rally): a few puffs, that hang longer
 * The rolling noise is the shoulder's own road-noise voice (race_sound.c's off-road branch), lower; no gravel one-shots.
 * 0, the default, is the exe's. */
#ifndef TNFS_RALLY_GRAVEL_H
#define TNFS_RALLY_GRAVEL_H

#include <stdint.h>

enum { RALLY_GRAVEL_OFF = 0, RALLY_GRAVEL_COMPACT, RALLY_GRAVEL_DUST, RALLY_GRAVEL_REGOLITH };

/* The kind of a race: RALLY_GRAVEL_OFF unless the race is a rally (`variant_r`) and `enabled`; `track` is race_sim_track_index's,
 * `lunar` extras.cfg's lunar_springs (the rally on Rusty Springs, track 3, is then the moon's). */
int rally_gravel_kind(int track, int variant_r, int lunar, int enabled);

/* The road noise's level (0..0x7f) as the rolling gravel gives it: lower than the exe's off-road scrape. `level` as race_sound.c's
 * road_noise has it before its limit. */
int32_t rally_gravel_level(int kind, int32_t level);

/* The shoulder's scrape (the road noise's off-road branch) in a rally with the switch: the exe's level is 0x14 + twice the speed in metres a second, which
 * at a crawl is faint (24 of 127 at 2 m/s) and the owner could not hear it. A car that rolls gets RALLY_SHOULDER_FLOOR at least, in full from 3 m/s and in proportion below (a standing one none); a standing
 * one and a fast one keep the exe's level; RALLY_GRAVEL_OFF too. `speed` as car->speed (16.16). */
enum { RALLY_SHOULDER_FLOOR = 0x70 };
int32_t rally_gravel_shoulder_level(int kind, int32_t level, int32_t speed);

/* The same for the road in a rally with the switch (the rolling gravel, rally_gravel_level's result): a car that rolls gets RALLY_ROAD_FLOOR at least, about
 * half the shoulder's, so the road is heard at a crawl and the shoulder stays the louder. */
enum { RALLY_ROAD_FLOOR = 0x50 };
int32_t rally_gravel_road_level(int kind, int32_t level, int32_t speed);

/* The rolling crunch: the exe's own gravel sample (race_sound.c's 0x50, a quiet impulsive crunch: the sound bank's one sample of the kind) struck on the rolling car as the exe strikes
 * it on a track 5 shoulder, at random, but oftener the faster the car goes. `speed` 16.16 metres a second (signed), `rand` 0..0xffff: the volume (1..0x7f) to strike it with this tick,
 * 0 for none (the switch off, a standing car). The strikes overlap (a fresh voice each), so they run into a roll. */
int32_t rally_gravel_crunch(int kind, int32_t speed, uint32_t rand);

/* How much dust a car rolling at `speed` (16.16 metres a second) on the ground throws: 0 for none (nonzero drops puffs, see dust_trail.h). Compacted
 * gravel and a standing car throw none here; the exe's own skid marks and smoke are left to the tyres. */
int32_t rally_dust_skid(int kind, int32_t speed, int on_ground);

#endif
