/* port (the PS1 look's light on the road): the polygon's darkness level, the street lamps' and windows' boards, the scene tones' and the lamps' tables, the reflectors' shine, taken out of ported_view.c: the draw hooks the track
 * is drawn through (draw_uv). ported_view.c makes the tables (light_tables_prepare) and hands the hooks to the track; what this file takes of the view is in view_shared.h. */

#include "game/race/reflector.h"
#include "game/race/track_mask.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/viewer/reflect_pass.h"
#include "game/viewer/street_halo.h"
#include "game/viewer/view_lamps.h"
#include "viewer/drive_lamps.h"
#include "viewer/view_tables.h"
#include "viewer/view_shared.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port (the PS1 look's light): the terrain polygon being drawn carries one darkness level, the mean of its corners'
 * intensities in the fog tables' 16 steps (0 = full light, nothing changes); a lit polygon is drawn through the mappers'
 * remap mode with that level's table, which the recorder keeps by pointer (the tables live until the next frame) */
uint8_t lit_tables[MODERN_FOG_LEVELS][256];
uint8_t pool_tables[4][MODERN_FOG_LEVELS][256];   /* port (lamp_pool_tint): the tables of a street lamp's pool, by class: the lit ones with a warm tint, the more the nearer the lamp */
int pool_ready;
uint8_t tunnel_tables[MODERN_FOG_LEVELS][256];   /* the same with the tunnel's cast, for the polygons all in a tunnel's rows */
int lit_level;
int tables_made;          /* the tables above have been made by a render (the mirror's follow pass draws with them) */
int lit_active;           /* a polygon or object of the light's is being drawn (between the hook's two calls) */
int lit_tinted;           /* the ambient colour is not neutral: even full light goes through the tables */
float lit_tint[3] = { 1, 1, 1 };
uint8_t lit_corner[4];   /* the polygon's corners' intensities, for the Modern 3D road's per-pixel gradient */


void light_hook(const uint8_t corners[4])
{
    lit_active = corners != NULL;
    if (corners == NULL) {
        lit_level = 0;
        memset(lit_corner, 255, sizeof lit_corner);
        return;
    }
    memcpy(lit_corner, corners, sizeof lit_corner);
    const int mean = (corners[0] + corners[1] + corners[2] + corners[3] + 2) / 4;
    lit_level = ((255 - mean) * (MODERN_FOG_LEVELS - 1) + 127) / 255;
}

const float NO_CAST[3] = { 1, 1, 1 };   /* (the tunnel's own cast and least light: psx_light_tunnel_look) */

/* port (night, psx_headlights; street_lamp.h): the street lamp's board, whose entries of its own the light tables leave alone as the lamps' colours are */
const FshEntry *street_bitmap[STREET_BOARDS];
int street_scenery[STREET_BOARDS], street_count;
uint8_t street_entry[3];
uint8_t street_tables[MODERN_FOG_LEVELS][256];

void ported_view_set_street_lamps(const int scenery[], int n, const uint8_t entry[3])
{
    street_count = entry != NULL && scenery != NULL && n > 0 ? (n < STREET_BOARDS ? n : STREET_BOARDS) : 0;
    for (int i = 0; i < STREET_BOARDS; i++) street_bitmap[i] = NULL;
    for (int i = 0; i < street_count; i++) street_scenery[i] = scenery[i];
    if (street_count > 0) memcpy(street_entry, entry, sizeof street_entry);
    street_halo_sync();
}

const FshEntry *window_bitmap[WINDOW_BOARDS_MAX];
int window_scenery[WINDOW_BOARDS_MAX], window_count;
uint8_t window_entry;
uint8_t window_tables[MODERN_FOG_LEVELS][256];

void ported_view_set_window_boards(const int scenery[], int n, uint8_t entry)
{
    window_count = scenery != NULL && n > 0 ? (n < WINDOW_BOARDS_MAX ? n : WINDOW_BOARDS_MAX) : 0;
    for (int i = 0; i < window_count; i++) window_scenery[i] = scenery[i], window_bitmap[i] = NULL;
    window_entry = entry;
    street_halo_sync();
}

/* port (lamp_halos): the halo's module is told of the lamp boards, their entries and the window entry whenever one changes */
void street_halo_sync(void) { street_halo_boards(street_bitmap, street_count, street_entry, window_entry); }


/* port (track light file, `tone`): the tables of a scene tone, one set for the open road and one for a tunnel's polygons, each made when its first polygon asks (a track has 16 tones at most:
 * 4 KB a set, about 2.5 ms to make) from what the table block below saved (the palette and the tints it was made with); the block drops them all when any of that changes */
static uint8_t tone_tables[PSX_LIGHT_TONES][2][MODERN_FOG_LEVELS][256];
uint8_t tone_made[PSX_LIGHT_TONES][2];
uint8_t tone_palette[768];
float tone_lit_tint[3], tone_tunnel_tint[3], tone_cast[3];
int tone_have_cast;
float tone_min_light;
float tone_mean;   /* the time's mean tint (drive_light_tint), which a zone's own light is taken over (psx_light_tunnel_look) */

/* the tint a tone gives: each channel 1 at no strength, the tone's own (0..1) at full */
static void tone_tint_of(const PsxLightTone *tone, float out[3])
{
    const float p = (float)tone->percent / 100.0f;
    for (int k = 0; k < 3; k++) out[k] = 1.0f + ((float)tone->rgb[k] / 255.0f - 1.0f) * p;
}

static uint8_t (*tone_get(int id, int in_tunnel))[256]
{
    if (psx_light == NULL || id < 0 || id >= psx_light->tone_count) return NULL;
    const PsxLightTone *const slot = &psx_light->tone[id];
    if (slot->look && !in_tunnel) return NULL;   /* port (tunnel zone looks): a zone's look is a tunnel's tables; a polygon that is not in a tunnel has the road's */
    uint8_t (*t)[256] = tone_tables[id][in_tunnel != 0];
    if (tone_made[id][in_tunnel != 0]) return t;
    if (slot->look) {   /* the zone's own light and colour, as the time's look is made (psx_light_tunnel_look): the light over the time's own tint, the colour as the cast */
        const float gain = (float)(tone_mean > 0.05f ? (double)slot->light / 100.0 / (double)tone_mean : (double)slot->light / 100.0);
        const float grey[3] = { gain, gain, gain };
        modern_tunnel_tables(tone_palette, grey, slot->cast, 0.0f, t);
        tone_made[id][1] = 1;
        return t;
    }
    float tint[3];
    tone_tint_of(&psx_light->tone[id], tint);
    if (in_tunnel && tone_have_cast) {   /* a tunnel's polygon: its own cast through the tone */
        const float cast[3] = { tone_cast[0] * tint[0], tone_cast[1] * tint[1], tone_cast[2] * tint[2] };
        modern_tunnel_tables(tone_palette, tone_tunnel_tint, cast, tone_min_light, t);
    } else {
        const float lit[3] = { tone_lit_tint[0] * tint[0], tone_lit_tint[1] * tint[1], tone_lit_tint[2] * tint[2] };
        modern_light_tables(tone_palette, lit, t);
    }
    tone_made[id][in_tunnel != 0] = 1;
    return t;
}

/* port (track light file, `lamp`): the tables a street lamp's light is: the lit tables with the lamp's colour ADDED (a light, not a tint: nothing is darkened), for a colour (to 1/LAMP_KEY_STEPS a channel, its strongest
 * channel 1) and a strength bucket (the most the lamps give there, in lamps at 100 percent). Made when a polygon asks (2 ms each), from what the table block saved for the tones (the palette and the lit tint), and kept
 * in a ring of LAMP_CACHE: plain lamps all give one colour, so a handful serve, but neons of other colours mixed in make every polygon's mean colour another one (a stretch of them wants some 60: at 12 every polygon
 * rebuilt a table, 45 ms a frame); the block drops them when it makes its own again. */
static const float LAMP_BUCKET[LAMP_BUCKETS] = { 1.0f, 2.0f, 3.0f, 5.0f, 10.0f };
/* port (`lamp`): no dither between the lit tables and the lamp's: the pool is graded in LAMP_STEPS even steps of the colour added (step s: s / LAMP_STEPS of it), a pixel taking the step its heat rounds to, as the
 * headlights' pool grades (cone_cells: levels, not a random choice). A slot holds a block per step, each made the first time a polygon needs it (`made` is a bit a step). */
typedef struct { uint8_t key[4]; uint32_t made; uint8_t tables[LAMP_STEPS][MODERN_FOG_LEVELS][256]; } LampSlot;
static LampSlot lamp_cache[LAMP_CACHE];
int lamp_cache_used, lamp_cache_next;
/* port (tunnel_lamps): the same for a tunnel's polygons, whose lamp tables are the tunnel's own with the colour added (a few colours only: the tunnel's lamps are all one) */
enum { TUNNEL_LAMP_CACHE = 8 };
static LampSlot tunnel_lamp_cache[TUNNEL_LAMP_CACHE];
int tunnel_lamp_cache_used, tunnel_lamp_cache_next;

/* the blocks of steps 1 .. steps for a colour and bucket, in one run (block s - 1 the step s); NULL when steps is 0. `tunnel`: the tunnel's tables with the colour added, not the open road's */
static const uint8_t (*lamp_add_get(const float rgb[3], int bucket, int steps, int tunnel))[256]
{
    LampSlot *const cache = tunnel ? tunnel_lamp_cache : lamp_cache;
    int *const used = tunnel ? &tunnel_lamp_cache_used : &lamp_cache_used, *const next = tunnel ? &tunnel_lamp_cache_next : &lamp_cache_next;
    const int size = tunnel ? TUNNEL_LAMP_CACHE : LAMP_CACHE;
    uint8_t key[4];
    for (int k = 0; k < 3; k++) key[k] = (uint8_t)(rgb[k] * LAMP_KEY_STEPS + 0.5f);
    key[3] = (uint8_t)bucket;
    int slot = -1;
    for (int i = 0; i < *used && slot < 0; i++)
        if (memcmp(cache[i].key, key, sizeof key) == 0) slot = i;
    if (slot < 0) {
        slot = *used < size ? (*used)++ : *next;
        if (slot == *next && *used == size) *next = (*next + 1) % size;
        memcpy(cache[slot].key, key, sizeof key);
        cache[slot].made = 0;
    }
    for (int s = 1; s <= steps; s++) {
        if (cache[slot].made >> s & 1) continue;
        float add[3];
        for (int k = 0; k < 3; k++) add[k] = 255.0f * LAMP_ADD * LAMP_BUCKET[bucket] * (float)key[k] / LAMP_KEY_STEPS * (float)s / LAMP_STEPS;
        if (tunnel && tone_have_cast) modern_tunnel_tables_add(tone_palette, tone_tunnel_tint, tone_cast, tone_min_light, add, cache[slot].tables[s - 1]);
        else modern_light_tables_add(tone_palette, tone_lit_tint, add, cache[slot].tables[s - 1]);
        cache[slot].made |= 1u << s;
    }
    return cache[slot].tables[0];
}

/* port (`lamp`): the heat of the lamps at the polygon being drawn's four corners (the sum of their falloffs at each corner's place in the world, 0 none; a bucket's share of 1), and the step tables of the lamps'
 * colour at the strength the polygon's strongest corner takes (as many as its hottest corner reaches). 0 when no lamp reaches it, or it is a tunnel's or a tone's: those keep their own tables. The light then
 * grades per pixel through the steps as the heat does between the corners. */
static int lamp_polygon_in(int tunnel, float heat[4], const uint8_t (**top)[256])
{
    if (psx_light == NULL || psx_light->lamp_n <= 0 || !track_light_xz_ok || ((track_light_tunnel & 7) == 1) != (tunnel != 0) || !tables_made) return 0;
    int idx[LAMP_NEAR];
    int n = psx_light_lamps_near(psx_light, track_light_node, idx, LAMP_NEAR);
    if (tunnel) {   /* port (tunnel_lamps): a tunnel's polygons take the tunnel's lamps only (the street lamps' light stays out of it) */
        int kept = 0;
        for (int i = 0; i < n; i++)
            if (psx_light->lamps[idx[i]].tunnel) idx[kept++] = idx[i];
        n = kept;
    }
    if (n == 0) return 0;
    float rgb[4][3], strongest = 0.0f, mean[3] = { 0, 0, 0 };
    for (int k = 0; k < 4; k++) {
        heat[k] = psx_light_lamp_at(psx_light, idx, n, track_light_xz[k][0], track_light_xz[k][1], rgb[k]);
        strongest = heat[k] > strongest ? heat[k] : strongest;
        for (int c = 0; c < 3; c++) mean[c] += rgb[k][c] * heat[k];
    }
    if (strongest < 0.02f) return 0;   /* a pool's last sliver is not worth the warm path */
    float top_channel = 0.0f;
    for (int c = 0; c < 3; c++) top_channel = mean[c] > top_channel ? mean[c] : top_channel;
    for (int c = 0; c < 3; c++) mean[c] /= top_channel;
    int bucket = 0;
    while (bucket < LAMP_BUCKETS - 1 && LAMP_BUCKET[bucket] < strongest) bucket++;
    float hottest = 0.0f;
    for (int k = 0; k < 4; k++) {
        heat[k] = heat[k] / LAMP_BUCKET[bucket] < 1.0f ? heat[k] / LAMP_BUCKET[bucket] : 1.0f;
        hottest = heat[k] > hottest ? heat[k] : hottest;
    }
    const int steps = (int)(hottest * LAMP_STEPS + 0.5f);
    if (steps < 1) return 0;   /* under half a step everywhere: the lit tables, as the pixels would take them */
    *top = lamp_add_get(mean, bucket, steps, tunnel);
    return 1;
}

int lamp_polygon(float heat[4], const uint8_t (**top)[256]) { return lamp_polygon_in(0, heat, top); }

/* port (tunnel_lamps): the same for a polygon of a tunnel (track_light_tunnel 1, no scene tone): the tunnel's lamps' pools, ADDED over the tunnel's own light */
int lamp_polygon_tunnel(float heat[4], const uint8_t (**top)[256]) { return lamp_polygon_in(1, heat, top); }

/* the tables of the polygon being drawn: its scene tone's (`tone`, above the class's 3 bits), a tunnel's (track_light_tunnel 1), a lamp pool's (2..5), the road's */
static uint8_t (*light_tables(void))[256]
{
    const int base = track_light_tunnel & 7, tone = track_light_tunnel >> 3;
    if (tone > 0) {
        uint8_t (*t)[256] = tone_get(tone - 1, base == 1);
        if (t != NULL) return t;
    }
    if (base == 1) return tunnel_tables;
    if (base >= 2 && pool_ready) return pool_tables[(base - 2) & 3];
    return base ? tunnel_tables : lit_tables;
}

const uint8_t (*lit_tables_for(const FshEntry *texture))[256]
{
    const int lamp = view_lamps_of(texture);
    for (int i = 0; i < street_count; i++)
        if (street_bitmap[i] != NULL && texture == street_bitmap[i]) return street_tables;
    for (int i = 0; i < window_count; i++)
        if (window_bitmap[i] != NULL && texture == window_bitmap[i]) return window_tables;
    return lamp >= 0 ? view_lamps_table(lamp) : light_tables();
}

const uint8_t *lit_table_for(const FshEntry *texture)
{
    return lit_active && (lit_level > 0 || lit_tinted) ? lit_tables_for(texture)[lit_level] : NULL;
}

const uint8_t *lit_table(void) { return lit_active && (lit_level > 0 || lit_tinted) ? light_tables()[lit_level] : NULL; }

/* port (reflectors): the masks come from the track (track_mask.h's track_mask_for, which the loader fills); the textures are registered once per loaded art, the lamps' state each frame */
static const uint8_t *(*reflect_mask_for)(const TrackTexture *t) = track_mask_for;
int reflect_lamps_on, reflect_dark;
double reflect_x, reflect_z, reflect_hx, reflect_hz;

void ported_view_set_reflect_masks(const uint8_t *(*mask_for)(const TrackTexture *t))
{
    reflect_mask_for = mask_for;
    reflect_pass_forget_art();
}

static void reflect_register(const TrackArt *art)
{
    const int count = reflect_pass_register_art(art, reflect_mask_for);
    if (count >= 0) port_log("reflectors: %d textures with a mask\n", count);
}

/* the frame's state, before the track is drawn */
void reflect_frame(const TrackArt *art)
{
    if (!port_reflectors() || art == NULL) {
        reflect_pass_forget_art();
        reflect_pass_frame(0, 0, 0, 1, 0, 0, NULL);
        return;
    }
    reflect_register(art);
    const LampEntries lamps = drive_lamps_get_entries();
    reflect_pass_set_entries(street_count > 0 ? street_entry : NULL, window_count > 0 ? window_entry : 0, lamps.brake_lit, lamps.position, lamps.headlight, lamps.reverse);
    reflect_pass_frame(reflect_lamps_on, reflect_x, reflect_z, reflect_hx, reflect_hz, reflect_dark, art->palette);
}

static int reflect_thin_now;   /* the thinning of the polygon reflect_level_here saw last */

/* port (reflectors, reflect_pass.h): the step of shine (0 none) of the polygon being drawn: an object at its own place, a road polygon at the mean of its corners' ground places */
int reflect_level_here(void)
{
    if (!reflect_pass_active()) return 0;
    double x, z, height = 0.0;
    if (track_port_object_at != NULL) {
        x = track_port_object_at[0] / 65536.0, z = track_port_object_at[2] / 65536.0;
        height = 1.0;   /* a sign's face is about this far over the foot the object is placed by */
    } else if (track_light_xz_ok) {
        x = (track_light_xz[0][0] + track_light_xz[1][0] + track_light_xz[2][0] + track_light_xz[3][0]) / 4.0;
        z = (track_light_xz[0][1] + track_light_xz[1][1] + track_light_xz[2][1] + track_light_xz[3][1]) / 4.0;
    } else return 0;
    const int gain = reflect_pass_gain(x, z, height);
    reflect_thin_now = reflector_thin(gain);
    return reflector_level(gain);
}

int reflect_thin_here(void) { return reflect_thin_now; }

/* 004f9e94 = 004254e8 -> 00425210: 00486950 / 00487180 */
static void draw_uv_base(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    view_lamps_note(texture, uv, screen);
    street_halo_note(texture, screen);
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) quad_draw_mode(user, texture, uv, screen, transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP, lit);
    else quad_draw(user, texture, uv, screen, transparent(texture));
}

void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    draw_uv_base(user, texture, uv, screen);
    if (!reflect_pass_active()) return;
    const FshEntry *copy = reflect_pass_copy(texture);
    const int level = copy != NULL ? reflect_level_here() : 0;
    if (level > 0) street_halo_note_lit(texture, screen);
    if (level > 0) quad_draw_mode(user, reflect_pass_thin_copy(copy, reflect_thin_here()), uv, screen, MAP_REMAP_TRANSPARENT, reflect_pass_table(level));   /* port (reflectors) */
}

int modern;  /* port: EXTRAS' Modern 3D (ported_view_set_modern) */

void ported_view_set_modern(int on) { modern = on != 0; }
