/* port: --drive's bumper view: EXTRAS' Bumper Mirror and Radar (drive_bumper.h) in the frame. */
#include "viewer/drive_session.h"
#include "game/sys/palette.h"
#include "game/sys/port_settings.h"

#include <string.h>

/* port: EXTRAS' Bumper Mirror: its own mirror's pass as 0044ce4b runs the cockpit's (mirror_frame), on the world
 * this frame streamed, with the cockpit mirror's colours and car rows; 0 without a picture */
int drive_bumper_mirror_render(const DriveMirrorWorld *mirror, int32_t mirror_count)
{
    bumper_mirror.enclosed_colour = drive_hud.mirror.enclosed_colour;
    bumper_mirror.property8_colour = drive_hud.mirror.property8_colour;
    memcpy(bumper_mirror.car_rows, drive_hud.mirror.car_rows, sizeof bumper_mirror.car_rows);
    /* its rows made again whenever its first segment moves: the bumper camera never asks (chase +0x3c), and the
     * cockpit mirror's pass clears the flag first */
    int32_t rows_again = 1;
    DriveMirrorWorld world = *mirror;
    world.rows_again = &rows_again;
    DriveMirrorFrame pass;
    drive_mirror_zoom(&bumper_mirror, drive_wide_bumper_zoom());
    if (!drive_mirror_begin(&bumper_mirror, &world, &pass)) return 0;
    const MirrorFrameOps o = drive_mirror_ops(&pass);
    o.view(o.ctx, 1);
    o.transform(o.ctx);
    o.select(o.ctx);
    o.sky(o.ctx);
    o.scene(o.ctx, mirror_count);
    o.flip(o.ctx);
    o.view(o.ctx, 0);
    return 1;
}

/* port: the bumper view's mirror (`mirror`: its picture drawn) and radar over the frame just composed, on the 640's
 * `pixels` or the wide screen's */
void drive_bumper_draw(const RaceState *race, uint8_t *pixels, int mirror)
{
    const int wide = drive_wide_active();
    if (mirror) {
        const ScreenPoint at = drive_bumper_mirror_at(wide ? drive_wide.width : SCREEN_W, bumper_mirror.width);
        drive_bumper_blit(wide ? drive_wide_canvas() : pixels, wide ? drive_wide.width : SCREEN_W, SCREEN_H,
                          bumper_mirror.picture + DRIVE_MIRROR_SHAPE_HEADER, bumper_mirror.width, bumper_mirror.height, at);
        drive_wide_hires_place(&bumper_mirror, at.x, at.y, bumper_mirror.width, bumper_mirror.height);
    }
    if (drive_bumper.radar && drive_radar_view(race->chase.kind, port_radar_all_views()) && drive_hud.police.screen == NULL && drive_hud.cockpit_ready) {
        const Cockpit *c = &drive_hud.cockpit;
        DriveBumperRadarArt art = { c->big_lit, c->big_dark, c->small_lit, c->small_dark, 0, 0, 0 };
        if (drive_hud.rgb != NULL) {  /* the case's greys: the race palette's nearest */
            const uint8_t *usable = palette_usable();
            art.light = (uint8_t)palette_nearest(drive_hud.rgb, usable, DRIVE_BUMPER_RADAR_LIGHT_GREY,
                                                 DRIVE_BUMPER_RADAR_LIGHT_GREY, DRIVE_BUMPER_RADAR_LIGHT_GREY);
            art.face = (uint8_t)palette_nearest(drive_hud.rgb, usable, DRIVE_BUMPER_RADAR_FACE_GREY,
                                                DRIVE_BUMPER_RADAR_FACE_GREY, DRIVE_BUMPER_RADAR_FACE_GREY);
            art.dark = (uint8_t)palette_nearest(drive_hud.rgb, usable, DRIVE_BUMPER_RADAR_DARK_GREY,
                                                DRIVE_BUMPER_RADAR_DARK_GREY, DRIVE_BUMPER_RADAR_DARK_GREY);
        }
        drive_bumper_radar(wide ? drive_wide_canvas() : pixels, wide ? drive_wide.width : SCREEN_W, SCREEN_H, &art,
                           race->unknown_4b92d4, drive_sim.police_lights, drive_bumper_radar_at(wide ? drive_wide.width : SCREEN_W));
    }
}
