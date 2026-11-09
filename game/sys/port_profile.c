/* port: see port_profile.h. */
#include "game/sys/port_profile.h"
#include "game/sys/port_log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const KEYS[PROFILE_COUNT] = { "stock", "restored", "restomod" };
static const char *const FILES[PROFILE_COUNT] = { "stock.cfg", "restored.cfg", "restomod.cfg" };
static const char *const TITLES[PROFILE_COUNT] = { "Stock '96", "Restored '96", "Restomod '26" };
static const char *const LINE1[PROFILE_COUNT] = { "Factory spec. Every bolt as it shipped.", "Original paint, new bearings.",
                                                 "Original block, period-correct mods." };
static const char *const LINE2[PROFILE_COUNT] = { "4:3, as it shipped in 1996. Nothing touched.", "Same look, widescreen at 144 Hz.",
                                                 "1996 renderer, 2026 power: night, lights, paint jobs." };

const char *port_profile_key(Profile p) { return p >= 0 && p < PROFILE_COUNT ? KEYS[p] : KEYS[PROFILE_STOCK]; }
const char *port_profile_file(Profile p) { return p >= 0 && p < PROFILE_COUNT ? FILES[p] : FILES[PROFILE_STOCK]; }
const char *port_profile_title(Profile p) { return p >= 0 && p < PROFILE_COUNT ? TITLES[p] : TITLES[PROFILE_STOCK]; }
const char *port_profile_line1(Profile p) { return p >= 0 && p < PROFILE_COUNT ? LINE1[p] : LINE1[PROFILE_STOCK]; }
const char *port_profile_line2(Profile p) { return p >= 0 && p < PROFILE_COUNT ? LINE2[p] : LINE2[PROFILE_STOCK]; }

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

/* what a mode sets, column by column; what has no column (laps, time of day, weather, threads, the view's field) is at port_settings_defaults in every mode */
typedef struct {
    int widescreen;   /* 1: the monitor's format, else 4:3 */
    int refresh_hz, view_ultra, perspective, texture_filter, fog, f3_ultra, render_scale;
    int psx_light, psx_headlights, psx_shadow, psx_flares, body_shine, car_colours, fog_sample, sky_fill;
    int movies_wide, psx_liveries, rally_gravel;   /* the mode's file's */
    int made_tunnels, hall_light, headlight_relight, night_stretch_clean;   /* port.cfg's, appended after the extras' three */
    int forest_dark;   /* port.cfg's, appended */
    int night_sky_hue;   /* port.cfg's, appended */
    int bumper_mirror, bumper_radar, follow_camera;   /* the mode's file's, appended */
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
    int night_tone, rain_lamps, gauge_exterior, sign_glow;   /* port.cfg's, appended */
    int traffic_spray;   /* port.cfg's, appended */
    int rally_dry_lamps;   /* port.cfg's, appended */
    int traffic_halo;   /* port.cfg's, appended */
    int police_halo;   /* port.cfg's, appended */
    int analog_pad;   /* port.cfg's, appended */
    int rain_v2;   /* port.cfg's, appended */
    int shoulder_light;   /* port.cfg's, appended */
    int track_light_lamp;   /* port.cfg's, appended */
    int rain_tyre_roll;
    int snow_weather;   /* port.cfg's, appended */
    int reflectors;   /* port.cfg's, appended */
    int handbrake_lamps;   /* port.cfg's, appended */
    int rally_spray;   /* port.cfg's, appended */
    int trail_rise;   /* port.cfg's, appended */
    int snow_fx;   /* port.cfg's, appended */
    int lamp_halos;   /* port.cfg's, appended */
    int spray_cd_smoke;   /* port.cfg's, appended */
    int wipers;   /* port.cfg's, appended */
    int water_mist;   /* port.cfg's, appended */
    int weather_fog;   /* port.cfg's, appended */
    int spray_v4;   /* port.cfg's, appended */
} Table;

static const Table TABLES[PROFILE_COUNT] = {
    [PROFILE_STOCK]    = { 0, 30, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },   /* 30: the exe's 30 steps a second, each drawn once */
    [PROFILE_RESTORED] = { 1, 144, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    [PROFILE_RESTOMOD] = { 1, 144, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 1, 5, 1, 1, 2, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 2, 0, 0 },   /* render_scale 2; body_shine 4: the envmap, the owner's pick */
};

/* what the player may change in a mode, by file key; Restomod '26 leaves everything free but the machine's own */
static const char *const FREE_IN_ALL[] = { "fullscreen", "explode", "nitro", "turbo", "warrior", "mirror", "rally", "vegas", "units", "update_check", NULL };
static const char *const FREE_IN_RESTORED[] = { "format", "refresh_hz", "scale", "render_bands", "stats_overlay", NULL };
static const char *const MACHINE[] = { "profile", "psx_dir", NULL };

static int listed(const char *const *list, const char *key)
{
    for (; *list != NULL; list++)
        if (strcmp(*list, key) == 0) return 1;
    return 0;
}

int port_profile_key_free(Profile p, const char *key)
{
    if (listed(MACHINE, key)) return 0;
    if (p == PROFILE_RESTOMOD) return 1;
    return listed(FREE_IN_ALL, key) || (p == PROFILE_RESTORED && listed(FREE_IN_RESTORED, key));
}

/* one line of `text` from `pos` into `line` (cut to its size, no line end); the position after it */
static size_t next_line(const char *text, size_t size, size_t pos, char *line, size_t room)
{
    size_t len = 0;
    while (pos < size && text[pos] != '\n') {
        if (len < room - 1) line[len++] = text[pos];
        pos++;
    }
    if (len > 0 && line[len - 1] == '\r') len--;
    line[len] = '\0';
    return pos + 1;
}

void port_profile_read_free(Profile p, const char *text, size_t size, void (*take)(void *to, const char *key, const char *value), void *to)
{
    for (size_t pos = 0; pos < size;) {
        char line[PSX_DIR_MAX + 32];
        pos = next_line(text, size, pos, line, sizeof line);
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        if (port_profile_key_free(p, line)) take(to, line, eq + 1);
    }
}

static int has_line(const char *text, const char *line)
{
    const size_t size = strlen(text);
    for (size_t pos = 0; pos < size;) {
        char other[PSX_DIR_MAX + 32];
        pos = next_line(text, size, pos, other, sizeof other);
        if (strcmp(other, line) == 0) return 1;
    }
    return 0;
}

long port_profile_differences(Profile p, const char *base, const char *now, const char *keep, char *out, size_t room, int *dropped)
{
    size_t n = 0;
    const size_t size = strlen(now);
    if (room > 0) out[0] = '\0';
    for (size_t pos = 0; pos < size;) {
        char line[PSX_DIR_MAX + 32], key[64];
        pos = next_line(now, size, pos, line, sizeof line);
        const char *eq = strchr(line, '=');
        if (eq == NULL || (size_t)(eq - line) >= sizeof key) continue;
        memcpy(key, line, (size_t)(eq - line));
        key[eq - line] = '\0';
        const int differs = !has_line(base, line);
        if (!port_profile_key_free(p, key)) {
            if (differs) {
                port_log("settings: %s is not free in %s, not saved\n", key, port_profile_key(p));
                *dropped += 1;
            }
            continue;
        }
        if (!differs && (keep == NULL || !has_line(keep, line))) continue;
        const size_t len = strlen(line);
        if (n + len + 2 > room) return -1;
        memcpy(out + n, line, len);
        n += len;
        out[n++] = '\n';
        out[n] = '\0';
    }
    return (long)n;
}

PortSettings port_profile_settings(Profile p, PortSettings s, ScreenFormat monitor)
{
    const Table *t = &TABLES[p >= 0 && p < PROFILE_COUNT ? p : PROFILE_STOCK];
    s.fullscreen = 1;   /* the three start full screen */
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
    s.chase_height = t->chase_height, s.lamp_pool_tint = t->lamp_pool_tint, s.trans_shadows = t->trans_shadows, s.canyon_shade = t->canyon_shade, s.tunnel_headlight = t->tunnel_headlight, s.parking_dark = t->parking_dark, s.forest_neutral = t->forest_neutral, s.cavern_dark = t->cavern_dark, s.vegas_objects = t->vegas_objects, s.close_houses = t->close_houses, s.brake_over_gas = t->brake_over_gas, s.lamp_pools = t->lamp_pools, s.lamp_car_shade = t->lamp_car_shade, s.reverse_rows = t->reverse_rows, s.city_lift = t->city_lift, s.vegas_lift = t->vegas_lift, s.headlight_cone = t->headlight_cone, s.rain_fx = t->rain_fx, s.rain_grip = t->rain_grip, s.gauge_light = t->gauge_light, s.finish_gate_light = t->finish_gate_light, s.lamp_pool_bright = t->lamp_pool_bright, s.night_tone = t->night_tone, s.rain_lamps = t->rain_lamps, s.gauge_exterior = t->gauge_exterior, s.sign_glow = t->sign_glow, s.analog_pad = t->analog_pad, s.rain_v2 = t->rain_v2, s.shoulder_light = t->shoulder_light, s.track_light_lamp = t->track_light_lamp, s.rain_tyre_roll = t->rain_tyre_roll, s.police_halo = t->police_halo, s.traffic_halo = t->traffic_halo, s.rally_dry_lamps = t->rally_dry_lamps, s.traffic_spray = t->traffic_spray;
    s.snow_weather = t->snow_weather;
    s.reflectors = t->reflectors;
    s.handbrake_lamps = t->handbrake_lamps;
    s.rally_spray = t->rally_spray;
    s.trail_rise = t->trail_rise;
    s.snow_fx = t->snow_fx;
    s.lamp_halos = t->lamp_halos;
    s.spray_cd_smoke = t->spray_cd_smoke;
    s.wipers = t->wipers, s.water_mist = t->water_mist, s.weather_fog = t->weather_fog, s.spray_v4 = t->spray_v4;
    return s;
}

PortExtras port_profile_extras(Profile p, PortExtras e)
{
    const Table *t = &TABLES[p >= 0 && p < PROFILE_COUNT ? p : PROFILE_STOCK];
    e.variant_time = p == PROFILE_RESTOMOD;   /* the time of day also with Mirror and Rally */
    e.reverse = p == PROFILE_RESTOMOD;   /* the track screen offers the way round */
    e.movies_wide = t->movies_wide, e.psx_liveries = t->psx_liveries, e.rally_gravel = t->rally_gravel;
    e.bumper_mirror = t->bumper_mirror, e.bumper_radar = t->bumper_radar, e.follow_camera = t->follow_camera;
    return e;
}
