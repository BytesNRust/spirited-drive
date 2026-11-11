/* port (the frame's set-up steps: the weather sky, the track stream and road style, the transform, the cars' list and the recorder start), taken out of ported_view.c.
 * ported_view.c calls these; what this file takes of the view is in view_shared.h. */

#include "game/gfx/raster_record.h"
#include "game/sys/bands.h"
#include "game/viewer/screen_layout.h"
#include "game/viewer/view_lamps.h"
#include "game/viewer/view_screen.h"
#include "game/viewer/weather_hooks.h"
#include "viewer/drive_light.h"
#include "viewer/sky_art_draw.h"
#include "viewer/view_light.h"
#include "viewer/view_spray.h"
#include "viewer/view_tables.h"
#include "viewer/view_weather.h"
#include "viewer/view_light_tables.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The storm's thunder and flash, and (port, weather) the sky toward its colour before the track covers it. */
void cars_weather_sky(const WeatherTables *wt, uint8_t *pixels, const int32_t box[4], View *view, const TrackCamera *cam)
{
    storm_thunder_update();
    rain_roll_update();
    snow_roll_update();
    drive_light_set_flash(wt != NULL ? storm_flash_level() : 0);   /* port (WEATHER_STORM): the palette goes bright with the flash, the sky white below */
    if (wt != NULL) {   /* port (weather): the sky toward its colour, before the track covers it */
        const WeatherSky sky = weather_sky(pixels, view_screen_w, box[0] < 0 ? 0 : box[0], box[2] > view_screen_w ? view_screen_w : box[2], wt,
                                           view, cam->matrix, storm_flash_level());
        bands_rows(box[1] < 0 ? 0 : box[1], box[3] < view_screen_h ? box[3] : view_screen_h, weather_sky_band, (void *)&sky);
    }
}

/* The frame's track streaming and the modern road's style (weather, fog or none). */
void cars_stream_style(const Tri *tri, const TrackArt *art, const PortedCamera *pc, const WeatherTables *wt)
{
    stream(tri, art, pc->node);
    if (!modern) modern_road_set_style(NULL);
    else if (wt != NULL) weather_style(art, wt, weather, snow_level(), port_weather_fog(), &look, look_filter, rain_open);
    else {
        modern_look_set_sample(&look, port_fog_sample(), port_fog_lift());
        modern_road_set_style(modern_look_style(&look, art, look_filter, look_fog, render.draw_segments));
    }
}

/* The headlights' pools, the track's rows transformed for the camera, and the models' view of it. */
void cars_transform(TrackCamera *cam, DrawContext *ctx, View *view)
{
    headlights_update();   /* port (headlight_beams): the pools of the cars with their lights on, before the rows are built */
    if (port_headlight_relight() || (light_effects != NULL && light_effects->pool != NULL && light_effects->pool->cone)) track_render_relight(&render);   /* port (headlight_relight): the road's light with the pools of this frame */
    track_render_transform(&render, cam, ctx);
    memcpy(last_near_dir, ctx->near_dir, sizeof last_near_dir);
    last_near = ctx->near;
    model_view = (ModelView){ view, cam->position, cam->matrix,
                              shadow_race != NULL && model_draw_half_side(shadow_race->track, shadow_race->track_variant), 1 };
}

/* The cars of the frame, and the object sink their shapes are drawn through. */
void cars_take_list(const TrackArt *art, const PortedCar *list, int count, QuadTarget *quad, View *view)
{
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].car = list[i].car;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    if (shadow_remap == NULL) object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ quad, art->palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(view) };
}

/* The frame's recording by bands (with threads), the lamps' and the light's tables, and the rain's spray. */
void cars_frame_begin(const TrackArt *art, const WeatherTables *wt, uint8_t *pixels)
{
    if (bands_threads() != 0) {
        raster_record_set_runner(bands_rows);
        raster_record_begin(&recorder, pixels);
    }
    if (light_effects == NULL && view_lamps_flares() && view_lamps_count() > 0) view_lamps_textures(art);
    if (light_effects != NULL) light_tables_prepare(art);
    reflect_frame(art);
    lit_level = 0;
    memset(lit_corner, 255, sizeof lit_corner);
    view_spray_frame(art, wt, weather_wet(weather));
}
