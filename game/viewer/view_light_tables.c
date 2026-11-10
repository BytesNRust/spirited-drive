/* port (the light's tables made again when the palette or tint moved), taken out of ported_view.c.
 * ported_view.c calls these; what this file takes of the view is in view_shared.h. */

#include "game/viewer/view_lamps.h"
#include "game/viewer/weather_hooks.h"
#include "viewer/drive_light.h"
#include "viewer/psx_sky_draw.h"
#include "viewer/view_spray.h"
#include "viewer/view_tables.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port: the lamps', the street lamps' and the lit windows' tables from the world's tables just made, and the textures they are drawn on */
static void lamp_street_window_tables(const TrackArt *art)
{
    view_lamps_make_tables(art, (const uint8_t (*)[256])tunnel_tables);   /* the lamps' textures and tables */
    if (street_count > 0) {   /* the street lamps' boards: their entries kept */
        uint8_t exempt[256] = { 0 };
        for (int k = 0; k < 3; k++) exempt[street_entry[k]] = 1;
        for (int i = 0; i < street_count; i++)
            street_bitmap[i] = street_scenery[i] >= 0 && street_scenery[i] < TRACK_SCENERY_TEXTURES && art->scenery[street_scenery[i]].present
                                   ? &art->scenery[street_scenery[i]].bitmap : NULL;
        psx_lamp_tables(exempt, (const uint8_t (*)[256])lit_tables, MODERN_FOG_LEVELS, street_tables);
        street_halo_sync();
    }
    if (window_count > 0) {   /* the lit windows' boards: their entry kept */
        uint8_t exempt[256] = { 0 };
        exempt[window_entry] = 1;
        psx_lamp_tables(exempt, (const uint8_t (*)[256])lit_tables, MODERN_FOG_LEVELS, window_tables);
        for (int i = 0; i < window_count; i++)
            window_bitmap[i] = window_scenery[i] >= 0 && window_scenery[i] < TRACK_SCENERY_TEXTURES && art->scenery[window_scenery[i]].present
                                   ? &art->scenery[window_scenery[i]].bitmap : NULL;
    }
}

/* port (PS1 light): what the tables in force were made from; they are made again only when one of these moved (a fade changes the palette). One unit: the tone cache below
 * (tone_*) is reset together with it */
static struct {
    uint8_t palette[768];
    float tint[3];
    PsxTunnelLook look;
    int made, pool;
    PsxLightTone tone[PSX_LIGHT_TONES];
    int tone_count;
} light_made;

/* port (lamp_pool_tint): the lit tables again with the pool's warm tint, by class */
static void pool_tables_make(const TrackArt *art)
{
    static const float WARM[3] = { 1.0f, 0.9f, 0.55f };
    for (int k = 0; k < 4; k++) {
        float tint[3];
        for (int c = 0; c < 3; c++) tint[c] = lit_tint[c] * (1.0f + (WARM[c] - 1.0f) * (float)(k + 1) / 4.0f);
        modern_light_tables(art->palette, tint, pool_tables[k]);
    }
}

/* port (PS1 light): the lit, tunnel and pool tables again, and the scene tones' and the lamps' caches reset from the same inputs */
static void light_tables_make(const TrackArt *art, const PsxTunnelLook *look, int has_pool)
{
    modern_light_tables(art->palette, lit_tint, lit_tables);
    memcpy(light_made.tone, psx_light->tone, sizeof light_made.tone), light_made.tone_count = psx_light->tone_count;   /* port (`tone`): the scene tones' tables are made again when asked, from these */
    memset(tone_made, 0, sizeof tone_made);
    lamp_cache_used = lamp_cache_next = 0;   /* port (`lamp`): the lamps' tables are made again from the palette */
    memcpy(tone_palette, art->palette, sizeof tone_palette);
    memcpy(tone_lit_tint, lit_tint, sizeof tone_lit_tint), memcpy(tone_tunnel_tint, look->tint, sizeof tone_tunnel_tint);
    tone_have_cast = look->cast_on, tone_min_light = look->min_light;
    memcpy(tone_cast, look->cast, sizeof tone_cast);
    if (look->cast_on) modern_tunnel_tables(art->palette, look->tint, look->cast, look->min_light, tunnel_tables);
    else modern_light_tables_cast(art->palette, lit_tint, NO_CAST, tunnel_tables);
    if (has_pool) pool_tables_make(art);
    pool_ready = has_pool;
}

/* port (PS1 light): the tables of this frame's light (the lit, tunnel, pool, tone, lamp, street and window tables), made again only when the palette or the tint moved (a fade changes the palette), and the polygon hook set */
void light_tables_prepare(const TrackArt *art)
{
    float shown[3];
    drive_light_tint(shown);
    PsxTunnelLook look;
    psx_light_tunnel_look(psx_light, shown, lit_tint, view_lamps_count(), &look);   /* the tunnel's tables: the file's look, else the made light's (psx_light.h) */
    const int has_pool = psx_light->pool_class != NULL;
    if (!light_made.made || light_made.pool != has_pool || light_made.tone_count != psx_light->tone_count || memcmp(light_made.tone, psx_light->tone, sizeof light_made.tone) != 0
        || memcmp(light_made.palette, art->palette, sizeof light_made.palette) != 0 || memcmp(light_made.tint, lit_tint, sizeof light_made.tint) != 0
        || memcmp(&light_made.look, &look, sizeof look) != 0) {
        light_made.look = look;
        tone_mean = (shown[0] + shown[1] + shown[2]) / 3.0f;
        light_tables_make(art, &look, has_pool);
        light_made.pool = has_pool;
        memcpy(light_made.palette, art->palette, sizeof light_made.palette), memcpy(light_made.tint, lit_tint, sizeof light_made.tint), light_made.made = 1;
    }
    lamp_street_window_tables(art);
    track_light_hook = light_hook;
    tables_made = 1;
}
