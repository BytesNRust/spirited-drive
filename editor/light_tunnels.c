/* See light_tunnels.h. */
#include "light_tunnels.h"

#include <stdio.h>
#include <string.h>

int light_tunnels_list(const LightFile *time, const LightFile *places, LightTunnelRow *out, int cap)
{
    int n = 0;
    const int zones = time != NULL ? light_file_count(time, LIGHT_KIND_ZONE) : 0, lamps = time != NULL ? light_file_count(time, LIGHT_KIND_TUNNEL_LAMP) : 0;
    unsigned char zone_used[LIGHT_MAX_TONES + 1] = { 0 }, lamp_used[LIGHT_MAX_TUNNEL_LAMPS + 1] = { 0 };
    const int place_count = places != NULL ? light_file_count(places, LIGHT_KIND_PLACE) : 0;
    int tunnel = 0;
    for (int i = 0; i < place_count && n < cap; ++i) {
        LightPlace p;
        if (!light_file_get_place(places, i, &p) || strcmp(p.what, "tunnel") != 0) continue;
        LightTunnelRow *r = &out[n++];
        *r = (LightTunnelRow){ p.from, p.to, tunnel++, -1, -1, 0 };
        for (int z = 0; z < zones && z <= LIGHT_MAX_TONES; ++z) {
            LightZone zone;
            if (light_file_get_zone(time, z, &zone) && zone.from == p.from && zone.to == p.to && !zone_used[z]) {
                r->zone = z, r->hall = zone.lit != 0, zone_used[z] = 1;
                break;
            }
        }
        for (int l = 0; l < lamps && l <= LIGHT_MAX_TUNNEL_LAMPS; ++l) {
            LightTunnelLamp lamp;
            if (light_file_get_tunnel_lamp(time, l, &lamp) && lamp.has_nodes && lamp.from == p.from && lamp.to == p.to && !lamp_used[l]) {
                r->lamp = l, lamp_used[l] = 1;
                break;
            }
        }
    }
    for (int z = 0; z < zones && n < cap; ++z) {   // the zones that are no place's tunnel: a tunnel of this time of day only
        LightZone zone;
        if (zone_used[z] || !light_file_get_zone(time, z, &zone)) continue;
        LightTunnelRow *r = &out[n++];
        *r = (LightTunnelRow){ zone.from, zone.to, -1, z, -1, zone.lit != 0 };
        zone_used[z] = 1;
        for (int l = 0; l < lamps && l <= LIGHT_MAX_TUNNEL_LAMPS; ++l) {
            LightTunnelLamp lamp;
            if (light_file_get_tunnel_lamp(time, l, &lamp) && lamp.has_nodes && lamp.from == zone.from && lamp.to == zone.to && !lamp_used[l]) {
                r->lamp = l, lamp_used[l] = 1;
                break;
            }
        }
    }
    for (int l = 0; l < lamps && n < cap; ++l) {   // a lamp line over nodes that are no tunnel's: its own row
        LightTunnelLamp lamp;
        if (lamp_used[l] || !light_file_get_tunnel_lamp(time, l, &lamp) || !lamp.has_nodes) continue;
        out[n++] = (LightTunnelRow){ lamp.from, lamp.to, -1, -1, l, 0 };
        lamp_used[l] = 1;
    }
    return n;
}

static void general_lamp(const LightFile *time, LightTunnelLook *out)
{
    out->strength = time->letter == 'N' ? 300.0 : 150.0, out->reach = 1.8;
    out->lamp_colour[0] = '\0';
    out->strength_own = out->reach_own = out->lamp_colour_own = 0;
    for (int i = 0; i < light_file_count(time, LIGHT_KIND_TUNNEL_LAMP); ++i) {   // the last general line
        LightTunnelLamp l;
        if (!light_file_get_tunnel_lamp(time, i, &l) || l.has_nodes) continue;
        out->strength = l.strength, out->strength_own = 1;
        out->lamp_colour_own = out->reach_own = 0;
        snprintf(out->lamp_colour, sizeof out->lamp_colour, "%s", l.has_colour ? l.colour : "");
        out->lamp_colour_own = l.has_colour;
        if (l.has_reach) out->reach = l.reach, out->reach_own = 1;
        else out->reach = 1.8;
    }
}

void light_tunnels_all(const LightFile *time, LightTunnelLook *out)
{
    memset(out, 0, sizeof *out);
    LightTunnelTone t;
    if (light_file_get_tunnel_tone(time, 0, &t)) {
        out->has_light = 1, out->light = t.light, out->light_own = 1;
        if (t.has_colour) snprintf(out->colour, sizeof out->colour, "%s", t.colour), out->colour_own = 1;
    }
    general_lamp(time, out);
}

void light_tunnels_look(const LightFile *time, const LightTunnelRow *row, LightTunnelLook *out)
{
    light_tunnels_all(time, out);
    out->light_own = out->colour_own = 0;   // what the general lines give is inherited here; what the row's own lines give is its own
    out->strength_own = out->reach_own = out->lamp_colour_own = 0;
    LightZone z;
    if (row->zone >= 0 && light_file_get_zone(time, row->zone, &z) && !z.lit) {
        if (z.has_light) out->has_light = 1, out->light = z.light, out->light_own = 1;
        if (z.has_colour) snprintf(out->colour, sizeof out->colour, "%s", z.colour), out->colour_own = 1;
    }
    LightTunnelLamp l;
    if (row->lamp >= 0 && light_file_get_tunnel_lamp(time, row->lamp, &l)) {
        out->strength = l.strength, out->strength_own = 1;
        if (l.has_colour) snprintf(out->lamp_colour, sizeof out->lamp_colour, "%s", l.colour), out->lamp_colour_own = 1;
        if (l.has_reach) out->reach = l.reach, out->reach_own = 1;
    }
}

int light_tunnels_own_look(LightFile *time, const LightTunnelRow *row, char *why, size_t cap)
{
    if (row->hall) return snprintf(why, cap, "a hall is lit by its own lamps: it has no look of its own"), 0;
    LightTunnelLook now;
    light_tunnels_all(time, &now);   // the general values: the new lines say what the tunnel has already
    if (row->zone < 0) {
        LightZone z;
        memset(&z, 0, sizeof z);
        z.from = row->from, z.to = row->to;
        if (now.has_light) z.has_light = 1, z.light = now.light;
        if (now.colour[0] != '\0') z.has_colour = 1, snprintf(z.colour, sizeof z.colour, "%s", now.colour);
        if (z.has_light || z.has_colour) {   // a zone line with neither says nothing; the lamp line below is the look then
            if (!light_file_set_zone(time, -1, &z, why, cap)) return 0;
        }
    }
    if (row->lamp < 0) {
        LightTunnelLamp l;
        memset(&l, 0, sizeof l);
        l.has_nodes = 1, l.from = row->from, l.to = row->to, l.strength = now.strength;
        if (now.lamp_colour[0] != '\0') l.has_colour = 1, snprintf(l.colour, sizeof l.colour, "%s", now.lamp_colour);
        if (now.reach_own) l.has_reach = 1, l.reach = now.reach;
        if (!light_file_set_tunnel_lamp(time, -1, &l, why, cap)) return 0;
    }
    return 1;
}

int light_tunnels_remove_own(LightFile *time, const LightTunnelRow *row)
{
    int removed = 0;
    if (row->lamp >= 0) removed |= light_file_remove(time, LIGHT_KIND_TUNNEL_LAMP, row->lamp);   // the two kinds number their lines apart: one removal moves no index of the other
    if (row->zone >= 0 && !row->hall) removed |= light_file_remove(time, LIGHT_KIND_ZONE, row->zone);
    return removed;
}
