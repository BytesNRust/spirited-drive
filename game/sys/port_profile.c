/* port: see port_profile.h. */
#include "game/sys/port_profile.h"

#include "game/platform/platform.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const KEYS[PROFILE_COUNT] = { "custom", "stock", "restored", "restomod" };
static const char *const TITLES[PROFILE_COUNT] = { "Custom", "Stock '96", "Restored '96", "Restomod '26" };
static const char *const LINE1[PROFILE_COUNT] = { "Your own mix of settings.", "Factory spec. Every bolt as it shipped.", "Original paint, new bearings.",
                                                 "Original block, period-correct mods." };
static const char *const LINE2[PROFILE_COUNT] = { "", "4:3, as it shipped in 1996. Nothing touched.", "Same look, widescreen at 144 Hz.",
                                                 "1996 renderer, 2026 power: night, lights, paint jobs." };

const char *port_profile_key(Profile p) { return p >= 0 && p < PROFILE_COUNT ? KEYS[p] : KEYS[PROFILE_CUSTOM]; }
const char *port_profile_title(Profile p) { return p >= 0 && p < PROFILE_COUNT ? TITLES[p] : TITLES[PROFILE_CUSTOM]; }
const char *port_profile_line1(Profile p) { return p >= 0 && p < PROFILE_COUNT ? LINE1[p] : LINE1[PROFILE_CUSTOM]; }
const char *port_profile_line2(Profile p) { return p >= 0 && p < PROFILE_COUNT ? LINE2[p] : LINE2[PROFILE_CUSTOM]; }

int port_profile_named(const char *name)
{
    char lower[16];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)tolower((unsigned char)name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    for (int p = 0; p < PROFILE_COUNT; p++)
        if (strcmp(lower, KEYS[p]) == 0) return p;
    return -1;
}

ScreenFormat port_profile_monitor_format(int width, int height)
{
    if (width <= 0 || height <= 0) return SCREEN_16_9;
    if (width * 10 >= height * 22) return SCREEN_21_9;   /* 2.2 and wider: 21:9 and the 32:9 around it */
    if (width * 10 >= height * 15) return SCREEN_16_9;   /* 16:9, and 16:10 (1.6) */
    return SCREEN_4_3;
}

/* what a profile sets: the picture and the look, not the player's own matters (full screen, laps, PS1 folder, time of day, weather, threads, view's field) */
typedef struct {
    int widescreen;   /* 1: the monitor's format, else 4:3 */
    int refresh_hz, view_ultra, perspective, texture_filter, fog, f3_ultra, render_scale;
    int psx_light, psx_headlights, psx_shadow, psx_flares, body_shine, car_colours, fog_sample, sky_fill;
    int movies_wide, psx_liveries, rally_gravel;   /* extras.cfg's */
    int made_tunnels, hall_light, headlight_relight, night_stretch_clean;   /* port.cfg's, appended after the extras' three */
    int forest_dark;   /* port.cfg's, appended */
    int night_sky_hue;   /* port.cfg's, appended */
    int bumper_mirror, bumper_radar, follow_camera;   /* extras.cfg's, appended */
    int scale_integer;   /* port.cfg's, appended: 1 whole multiples only, 0 fill */
    int last_camera, replay_no_ffb;   /* port.cfg's, appended */
    int sky_fill_below;   /* port.cfg's, appended */
    int radar_all_views;   /* port.cfg's, appended */
    int tach_dim;   /* port.cfg's, appended */
    int gauge_wheel;   /* port.cfg's, appended */
    int police_glow;   /* port.cfg's, appended */
    int car_tunnel_light, vegas_car_night, rally_gravel_shoulder, chase_height, lamp_pool_tint, trans_shadows;   /* port.cfg's, appended */
    int canyon_shade, tunnel_headlight, parking_dark, forest_neutral, cavern_dark, vegas_objects, close_houses;   /* port.cfg's, appended */
    int brake_over_gas, lamp_pools, lamp_car_shade, reverse_rows, city_lift, vegas_lift, headlight_cone, rain_fx, rain_grip, gauge_light, finish_gate_light, lamp_pool_bright;   /* port.cfg's, appended */
    int night_tone;   /* port.cfg's, appended */
} Table;

static const Table TABLES[PROFILE_COUNT] = {
    [PROFILE_STOCK]    = { 0, 30,  0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },   /* 30: the exe's 30 steps a second, each drawn once */
    [PROFILE_RESTORED] = { 1, 144, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    [PROFILE_RESTOMOD] = { 1, 144, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 0, 1, 0, 2 },   /* render_scale 2; body_shine 4: the envmap, the owner's pick */
};

static int controlled(Profile p) { return p > PROFILE_CUSTOM && p < PROFILE_COUNT; }

PortSettings port_profile_settings(Profile p, PortSettings s, ScreenFormat monitor)
{
    if (!controlled(p)) return s;
    const Table *t = &TABLES[p];
    s.format = t->widescreen ? monitor : SCREEN_4_3;
    s.refresh_hz = t->refresh_hz, s.view_ultra = t->view_ultra, s.perspective = t->perspective, s.texture_filter = t->texture_filter, s.fog = t->fog;
    s.f3_ultra = t->f3_ultra, s.render_scale = t->render_scale, s.psx_light = t->psx_light, s.psx_headlights = t->psx_headlights, s.psx_shadow = t->psx_shadow;
    s.psx_flares = t->psx_flares, s.body_shine = t->body_shine, s.car_colours = t->car_colours, s.fog_sample = t->fog_sample, s.sky_fill = t->sky_fill;
    s.made_tunnels = t->made_tunnels, s.hall_light = t->hall_light, s.headlight_relight = t->headlight_relight, s.night_stretch_clean = t->night_stretch_clean;
    s.forest_dark = t->forest_dark;
    s.night_sky_hue = t->night_sky_hue;
    s.scale_integer = t->scale_integer;
    s.last_camera = t->last_camera, s.replay_no_ffb = t->replay_no_ffb;
    s.sky_fill_below = t->sky_fill_below;
    s.radar_all_views = t->radar_all_views;
    s.tach_dim = t->tach_dim;
    s.gauge_wheel = t->gauge_wheel;
    s.police_glow = t->police_glow;
    s.car_tunnel_light = t->car_tunnel_light, s.vegas_car_night = t->vegas_car_night, s.rally_gravel_shoulder = t->rally_gravel_shoulder;
    s.chase_height = t->chase_height, s.lamp_pool_tint = t->lamp_pool_tint, s.trans_shadows = t->trans_shadows, s.canyon_shade = t->canyon_shade, s.tunnel_headlight = t->tunnel_headlight, s.parking_dark = t->parking_dark, s.forest_neutral = t->forest_neutral, s.cavern_dark = t->cavern_dark, s.vegas_objects = t->vegas_objects, s.close_houses = t->close_houses, s.brake_over_gas = t->brake_over_gas, s.lamp_pools = t->lamp_pools, s.lamp_car_shade = t->lamp_car_shade, s.reverse_rows = t->reverse_rows, s.city_lift = t->city_lift, s.vegas_lift = t->vegas_lift, s.headlight_cone = t->headlight_cone, s.rain_fx = t->rain_fx, s.rain_grip = t->rain_grip, s.gauge_light = t->gauge_light, s.finish_gate_light = t->finish_gate_light, s.lamp_pool_bright = t->lamp_pool_bright, s.night_tone = t->night_tone;
    return s;
}

PortExtras port_profile_extras(Profile p, PortExtras e)
{
    if (!controlled(p)) return e;
    const Table *t = &TABLES[p];
    e.movies_wide = t->movies_wide, e.psx_liveries = t->psx_liveries, e.rally_gravel = t->rally_gravel;
    e.bumper_mirror = t->bumper_mirror, e.bumper_radar = t->bumper_radar, e.follow_camera = t->follow_camera;
    return e;
}

int port_profile_settings_match(Profile p, const PortSettings *s, ScreenFormat monitor)
{
    if (!controlled(p)) return 1;
    const PortSettings want = port_profile_settings(p, *s, monitor);
    return s->format == want.format && s->refresh_hz == want.refresh_hz && s->view_ultra == want.view_ultra && s->perspective == want.perspective &&
           s->texture_filter == want.texture_filter && s->fog == want.fog && s->f3_ultra == want.f3_ultra && s->render_scale == want.render_scale &&
           s->psx_light == want.psx_light && s->psx_headlights == want.psx_headlights && s->psx_shadow == want.psx_shadow && s->psx_flares == want.psx_flares &&
           s->body_shine == want.body_shine && s->car_colours == want.car_colours && s->fog_sample == want.fog_sample && s->sky_fill == want.sky_fill &&
           s->made_tunnels == want.made_tunnels && s->hall_light == want.hall_light && s->headlight_relight == want.headlight_relight &&
           s->night_stretch_clean == want.night_stretch_clean &&
           s->forest_dark == want.forest_dark &&
           s->night_sky_hue == want.night_sky_hue && s->scale_integer == want.scale_integer &&
           s->last_camera == want.last_camera && s->replay_no_ffb == want.replay_no_ffb && s->sky_fill_below == want.sky_fill_below && s->radar_all_views == want.radar_all_views && s->tach_dim == want.tach_dim && s->gauge_wheel == want.gauge_wheel &&
           s->police_glow == want.police_glow &&
           s->car_tunnel_light == want.car_tunnel_light && s->vegas_car_night == want.vegas_car_night && s->rally_gravel_shoulder == want.rally_gravel_shoulder &&
           s->chase_height == want.chase_height && s->lamp_pool_tint == want.lamp_pool_tint && s->trans_shadows == want.trans_shadows && s->canyon_shade == want.canyon_shade && s->tunnel_headlight == want.tunnel_headlight && s->parking_dark == want.parking_dark && s->forest_neutral == want.forest_neutral && s->cavern_dark == want.cavern_dark && s->vegas_objects == want.vegas_objects && s->close_houses == want.close_houses && s->brake_over_gas == want.brake_over_gas && s->lamp_pools == want.lamp_pools && s->lamp_car_shade == want.lamp_car_shade && s->reverse_rows == want.reverse_rows && s->city_lift == want.city_lift && s->vegas_lift == want.vegas_lift && s->headlight_cone == want.headlight_cone && s->rain_fx == want.rain_fx && s->rain_grip == want.rain_grip && s->gauge_light == want.gauge_light && s->finish_gate_light == want.finish_gate_light && s->lamp_pool_bright == want.lamp_pool_bright && s->night_tone == want.night_tone;
}

int port_profile_extras_match(Profile p, const PortExtras *e)
{
    if (!controlled(p)) return 1;
    const PortExtras want = port_profile_extras(p, *e);
    return e->movies_wide == want.movies_wide && e->psx_liveries == want.psx_liveries && e->rally_gravel == want.rally_gravel &&
           e->bumper_mirror == want.bumper_mirror && e->bumper_radar == want.bumper_radar && e->follow_camera == want.follow_camera;
}

/* the Options screen's Cancel: port.cfg and extras.cfg as they were before the first profile step, kept in memory (the files are tiny) */
static struct {
    int armed, taken;
    uint8_t *file[2];
    size_t size[2];
} undo;
static const char *const UNDO_NAMES[2] = { "port.cfg", "extras.cfg" };

static void undo_free(void)
{
    for (int i = 0; i < 2; i++) free(undo.file[i]), undo.file[i] = NULL, undo.size[i] = 0;
    undo.armed = undo.taken = 0;
}

void port_profile_undo_begin(void)
{
    undo_free();
    undo.armed = 1;
}

void port_profile_undo_note(void)
{
    if (!undo.armed || undo.taken) return;
    for (int i = 0; i < 2; i++) {
        size_t n = 0;
        uint8_t *data = platform_read_user_file(UNDO_NAMES[i], &n);
        if (data != NULL && n > 0) {
            undo.file[i] = malloc(n);
            if (undo.file[i] != NULL) memcpy(undo.file[i], data, n), undo.size[i] = n;
        }
        if (data != NULL) platform_free_file(data);
    }
    undo.taken = 1;
}

int port_profile_undo_restore(void)
{
    int ok = 1, restored = undo.taken;
    for (int i = 0; undo.taken && i < 2; i++) {
        static const uint8_t NONE[] = "\n";   /* a file that was not there: an empty one reads as the defaults, with no profile */
        ok &= platform_write_user_file(UNDO_NAMES[i], undo.file[i] != NULL ? undo.file[i] : NONE, undo.file[i] != NULL ? undo.size[i] : 1) ? 1 : 0;
    }
    undo_free();
    return restored ? (ok ? 1 : -1) : 0;
}

void port_profile_undo_end(void) { undo_free(); }
