/* port: the light effects' light for the two tracks the PS1 CD holds none for (Burnt Sienna, tr6, whose ZTR6 has other nodes; Transtropolis, tr7, with no Z file),
 * made at the race's start from the track's own road: nothing of the CD is read, nothing is written. The TRI marks a tunnel's nodes (the road property byte,
 * bit 4: CY1's run is nodes 1064..1171, the rows the CD's LGT darkens at 1063..1174); a run of LIGHT_SYNTH_MIN_RUN nodes or more is a tunnel, a shorter one
 * (4 or 8 nodes: an overpass or a bridge's shade, CY1 500 / 508) is not. A tunnel's rows are the CD's, as measured on CY1 (A, B and C alike where not said): the
 * points beyond the wall foot 0, the road and the wall foot the time's values, and at each door the rows step from the open road's 255 to those along the
 * measured doors' shapes: the points beyond over a few nodes, steep in the middle at the entry and slow at the start at the exit, the foot and the road over
 * LIGHT_SYNTH_FOOT_RAMP nodes in a smooth S. */
#ifndef TNFS_LIGHT_SYNTH_H
#define TNFS_LIGHT_SYNTH_H

#include "formats/tri.h"
#include "game/race/track_node_light.h"

#include <stdint.h>

enum {
    LIGHT_SYNTH_PROPERTY_TUNNEL = 4,   /* TriNode.property bit: a tunnel (CY1 1064..1171, TR6 199..546) */
    LIGHT_SYNTH_PROPERTY_OPEN = 1,     /* with the tunnel bit: not a tunnel (TR7 778..818, an open truss bridge: the sky shows through its sides) */
    LIGHT_SYNTH_PROPERTY_HALL = 16,    /* TriNode.property bit: a hall, a roofed straight lit by lamps like a tunnel (TR7's steelworks, 1011..1023 and 0..62, property 17) */
    LIGHT_SYNTH_FLAG_TUNNEL = 1,       /* light_synth_tunnels' flag of a tunnel's node */
    LIGHT_SYNTH_FLAG_HALL = 2,         /* and of a hall's: not darkened (the pours, the girders keep their light) but the cars and the dash take the lamp alternation */
    LIGHT_SYNTH_LOOK_RAMP = 6,         /* the nodes outside a tunnel's mouth over which a car takes its zone's look (light_synth_car_tunnel with a look): the weather's mouth, WEATHER_MOUTH_NODES */
    LIGHT_SYNTH_LAMP_GAP_SMOOTH_PERCENT = 75, /* port (car_tunnel_match): the smoothed gap between two lamps (odd nodes; 1.0 under one): the alternation stays, the dark part at 75 in place of 40 (owner, i173), so between two lamps the car is not far below the unlit open road at night, which is the level of a tunnel_tone of about 100 (measured, CY1N) */
    LIGHT_SYNTH_LAMP_GAP_PERCENT = 40, /* a tunnel's lamp alternation: the car between two lamps (odd nodes) has this percent of the light it has under one (even nodes) */
    LIGHT_SYNTH_CAR_RAMP = 3,          /* the nodes outside a tunnel's mouth over which the car's light comes back (light_synth_car_tunnel) */
    LIGHT_SYNTH_HALL_POINT = 4,        /* the row point the car's alternation reads (light_synth_car) */
    LIGHT_SYNTH_HALL_MARK = 254,       /* a hall node's value there: 254 of 255 to the world, the mark of a lit hall to the cars */
    LIGHT_SYNTH_MIN_RUN = 12,          /* nodes: the shortest run that is a tunnel */
    LIGHT_SYNTH_FOOT_RAMP = 9,         /* nodes either side of a tunnel over which the wall foot and the road go to their tunnel values */
    LIGHT_SYNTH_BEYOND_ENTRY = 7,      /* nodes before a tunnel over which the points beyond the foot go dark (CY1: 220 at 12, 192 at 6, 99 at 4, 11 at 3, 0 from 1) */
    LIGHT_SYNTH_BEYOND_EXIT = 10       /* and after it over which they come back (CY1: 0 to 2, 12, 30, 125, 162, 190, 201, 206, 215) */
};

/* the time of day the rows are of: the PS1 CD's files A, B and C */
enum { LIGHT_SYNTH_DAWN = 0, LIGHT_SYNTH_DAY = 1, LIGHT_SYNTH_DUSK = 2, LIGHT_SYNTH_NIGHT = 3 };   /* the night: the dusk's doors, a tunnel's road and foot at 255 */

/* 1 on node `i` of `property` (`count` bytes, a ring when `ring`) when it is in a run of LIGHT_SYNTH_MIN_RUN or more nodes with the tunnel bit; `flags` gets
 * 0 or 1 per node. A ring joins a run across node 0. */
/* port (rain_fx): 1 on the nodes of a run of LIGHT_SYNTH_MIN_RUN or more with a roof over the road for the rain: the tunnel bit, open to the side or not (Coastal's galleries, property 7: the base
 * 3 with the tunnel bit; the bare 5 of TR7's truss bridge is open sky). The lighting does not use it. */
void light_synth_rain_cover(const uint8_t *property, uint32_t count, int ring, uint8_t *flags);
void light_synth_tunnels(const uint8_t *property, uint32_t count, int ring, uint8_t *flags);

/* port (made_tunnels, car_tunnel_light): the same with `gallery` 1 also marking the runs of property 7 (CL2's galleries: a roof with one side open) */
void light_synth_tunnels_ex(const uint8_t *property, uint32_t count, int ring, int gallery, uint8_t *flags);

/* 1 for the track whose galleries (property 7) are tunnels of the CD's light: CL2 */
int light_synth_gallery_track(const char *track);

/* The rows of a light for the nodes with `flags` (count of them) at `time` (LIGHT_SYNTH_*): a tunnel's rows, the doors', the open road's 255. `data` is
 * count * TRACK_NODE_LIGHT_POINTS bytes. CY1's tunnel inside: road 255 by day, 215 at dawn, 214 at dusk; wall foot 130, 196, 106 (dusk's two sides, 14 and 198, averaged). */
void light_synth_rows(const uint8_t *flags, uint32_t count, int ring, int time, uint8_t *data);

/* A TrackNodeLight of the track's tunnels (its nodes node_count, the data owned: track_node_light_free). 1 when the track has a tunnel, else 0 and `out` is untouched. */
int light_synth(const Tri *tri, int time, TrackNodeLight *out);

/* as light_synth, with `gallery` 1 the galleries of light_synth_tunnels_ex count */
int light_synth_ex(const Tri *tri, int time, int gallery, TrackNodeLight *out);

/* port (made_tunnels): the road's tunnel nodes of `tunnels` (light_synth's, its `tunnel` flags taken over) become `made`'s own: each takes the tunnel's rows (under a roof
 * the terrain's light does not fall) and is flagged, except a hall's (made->hall) and the nodes hall_from..hall_to (an empty range when hall_from > hall_to), which stay as the
 * made light has them. */
void light_synth_made_tunnels(TrackNodeLight *made, TrackNodeLight *tunnels, uint32_t hall_from, uint32_t hall_to);

/* 1 for the tracks it is for: "TR6" and "TR7" (any case). */
int light_synth_track(const char *track);

/* The foundry hall of `track` (TR7: nodes 471..570 of the tunnel run 471..609, an open hall whose girders and pillars show the scenery's light; the run's last nodes are a
 * closed tunnel): 1 and the first and last node in `from` and `to` (inclusive), else 0. With port_hall_light the made light leaves the hall's points beyond the foot at the
 * terrain's own light, where a tunnel's rows darken them, so the car and the shadow there follow the scenery. */
int light_synth_hall(const char *track, uint32_t *from, uint32_t *to);

/* port (trans_shadows): Transtropolis' (TR7) airport approach to the open truss bridge, nodes 700..776: the terrain's walls shaded the scenery beside an open road in the made light.
 * 1 and the range (inclusive) for TR7, else 0. */
int light_synth_trans(const char *track, uint32_t *open_from, uint32_t *open_to);

/* port (trans_shadows): Transtropolis' (TR7) closed parking, the other way round: its deck roofs the road from node 465 (the sky shows to 460, the facade 460..464) but the road property marks the tunnel only from 471
 * (to 609, where the road is open again), so nodes 465..470 under the deck got the open light. 1 and the range (inclusive) for TR7, else 0; `row_node` is a node inside
 * the marked run whose tunnel row is the interior shade the range takes. */
int light_synth_trans_closed(const char *track, uint32_t *from, uint32_t *to, uint32_t *row_node);

/* port (parking_dark): Transtropolis' (TR7) whole parking, from the deck's first node (465) to the node before the exit tunnel (570, the hall's end): the car keeps one dark tone there.
 * 1 and the range (inclusive) for TR7, else 0. */
int light_synth_parking(const char *track, uint32_t *from, uint32_t *to);

/* port (cavern_dark): Burnt Sienna's (TR6) cavern, from the node after the blue-lit mine tunnel's mouth (322) to the road's tunnel run's end (546, the canyon's property-16 stretch is 547..): the car keeps one dim tone
 * there. 1 and the range (inclusive) for TR6, else 0. */
int light_synth_cavern(const char *track, uint32_t *from, uint32_t *to);

/* port (made_tunnels): 1 for the track whose tunnel is the blue mine (TR6, Burnt Sienna), whose rock keeps its own hue under the made tunnel tables instead of the warm cast. */
/* 1 for CY1, 2 for CY2 and CY3, 3 for TR3, 0 for any other track: the tunnels the CD's light colours orange or yellow */
int light_synth_city_track(const char *track);

int light_synth_mine(const char *track);

/* port (made_tunnels): the look of the warm tunnels of `kind` (light_synth_city_track: 1 CY1, 2 CY2 and CY3, 3 TR3) by day (`dark` 0) or at night (1), as the CD's light shows them, measured on the disc's
 * frames against the made ones: the cast of the dim levels (red, green, blue) and the percent of the made tunnel's light. 1 for a kind, else 0 and the outputs untouched. */
int light_synth_warm_look(int kind, int dark, float cast[3], int *percent);

/* The light a car and the dash have at `node` of a synthesised light, as a multiplier of each channel (`out`, 3): 1 on the open road, in a tunnel the CD's lamp
 * alternation (a RAMP context takes its first colour on even nodes and its second on odd ones: CY1's 266..292 is 128,115,96 then 60,60,60 through the exe's
 * table, 0.60 0.56 0.50 under a lamp, 0.39 grey between), which flickers yellowish with the speed; the way the points beyond the foot go dark at the doors,
 * the light goes between the open road's 1 and the alternation. A NULL light is 1. */
void light_synth_car(const TrackNodeLight *light, uint32_t node, float out[3]);

/* port (car_tunnel_light): the same on a made light that has the road's tunnel flags (TrackNodeLight.tunnel), but the car's darkness is the flags' alone: 1 in a tunnel (or a hall), ramping
 * to 0 over LIGHT_SYNTH_CAR_RAMP nodes outside either mouth. The rows' own points beyond the foot carry the terrain's shade as well (a canyon darkened the car into the tunnel's
 * light and lit its lamps by day) and ramp over ten nodes at the exit (the car stayed dark metres past the door). Without flags it is light_synth_car.
 * A light with a look (look_set: a file's, or a made one given the look it would be exported with) gives the car the zone's own light and colour in EVERY tunnel: the ratio under a lamp (even nodes) is
 * light% / 100 x colour x the road's light at the node (track_node_light_road_light) per channel, above full light too (the excess over 1 comes in with the road's darkening, as the world's does), between two lamps (odd) LIGHT_SYNTH_LAMP_GAP_PERCENT of that, and it ramps in over LIGHT_SYNTH_LOOK_RAMP nodes outside the mouth (the nearest tunnel node's zone gives
 * the look). A hall keeps its own lamps' factors and car_dim its dim one. */
void light_synth_car_tunnel(const TrackNodeLight *light, uint32_t node, float out[3]);

/* port (car_tunnel_match): light_synth_car_tunnel with the car matched to the world's light at the same tunnel_tone, for a light with a look. `match` 0 is the function above; 1 the world's
 * gain (the look over `mean`, the night's tint mean, as track_node_light_tunnel_look gives the tables; a mean under 0.05 divides nothing), the car's share of the light full light's (the road's
 * TRACK_NODE_LIGHT_TUNNEL_ROAD scaled up to TRACK_NODE_LIGHT_FULL inside the tunnel) and the lamp alternation kept soft: 1.0 under a lamp, LIGHT_SYNTH_LAMP_GAP_SMOOTH_PERCENT between two. The open road is 1. */
void light_synth_car_tunnel_match(const TrackNodeLight *light, uint32_t node, int match, float mean, float out[3]);

#endif
