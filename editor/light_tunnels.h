/* The Track light window's one Tunnels list (tunnels part 2 of specs/2026-11-22-tnfs-track-light-tuning-design.md): the tunnels of the track's places file (`tunnel FROM TO`), each with the
 * own look the time's file gives it, if it does. A tunnel's own look is the two lines of the time's file over exactly its nodes: `tunnel FROM TO [light] [colour]` (its ambient) and
 * `tunnel_lamp FROM TO STRENGTH [colour] [reach]` (its lamps). A `tunnel` or `tunnel_lamp` line whose nodes are no place's tunnel (an old retouch, or a tunnel that exists only at this time of
 * day) is not lost or reinterpreted: it is a row of its own, `place` -1. The general values (`All`) are the time's `tunnel_tone` and its `tunnel_lamp` line with no nodes. Pure C. */
#ifndef TNFS_EDITOR_LIGHT_TUNNELS_H
#define TNFS_EDITOR_LIGHT_TUNNELS_H

#include "light_file.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { LIGHT_TUNNEL_ROWS = 160 };

typedef struct {
    unsigned from, to;
    int place;   /* the index of the places file's `tunnel` line (among the tunnels, from 0), -1 for a row that exists at this time of day only */
    int zone;    /* the index of the time's `tunnel` zone line over exactly from..to, -1 for none */
    int lamp;    /* the index of the time's tunnel_lamp line over exactly from..to, -1 for none */
    int hall;    /* the zone says `lit` */
} LightTunnelRow;

/* The rows: the places file's tunnels in its order (`places` may be NULL: none), then the time's zone and tunnel_lamp lines that match none of them, in the file's order. Returns how many
 * (at most `cap`). */
int light_tunnels_list(const LightFile *time, const LightFile *places, LightTunnelRow *out, int cap);

/* What a row shows and where each value comes from: the ambient light (percent; `has_light` 0 when the file says nothing and the game's own look stays), its colour as written ("" for the game's),
 * and the lamps' strength, colour and reach. `*_own` is 1 when the row's own lines say it, 0 when it is inherited (from the general lines, or the game's values: 300 percent at night and 150 by
 * day, the warm colour, 1.8 nodes). */
typedef struct {
    int has_light, light_own;
    double light;
    char colour[48];
    int colour_own;
    double strength, reach;
    int strength_own, reach_own;
    char lamp_colour[48];
    int lamp_colour_own;
} LightTunnelLook;
void light_tunnels_look(const LightFile *time, const LightTunnelRow *row, LightTunnelLook *out);
/* the same for the general row: the tunnel_tone and the general tunnel_lamp line */
void light_tunnels_all(const LightFile *time, LightTunnelLook *out);

/* Own look: the row's zone and tunnel_lamp lines made (the ones it has stay) with the general values, so nothing changes until they are edited. Not for a hall. 1, or 0 with `why`. */
int light_tunnels_own_look(LightFile *time, const LightTunnelRow *row, char *why, size_t cap);
/* Remove own look: the row's zone and lamp lines dropped (a hall keeps its zone: it is not a look). 1, or 0 when it had none. */
int light_tunnels_remove_own(LightFile *time, const LightTunnelRow *row);

#ifdef __cplusplus
}
#endif

#endif
