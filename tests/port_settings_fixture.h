/* The shared fixture of the port settings' tests (test_port_settings.c, test_port_settings_values.c, test_port_settings_session.c): two in-memory
 * user files in place of the platform's storage, and a field by field comparison of two PortSettings. Each test file includes it once (it
 * defines the platform's file functions). */
#ifndef TNFS_TEST_PORT_SETTINGS_FIXTURE_H
#define TNFS_TEST_PORT_SETTINGS_FIXTURE_H

#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/race/session_rules.h"
#include "game/race/weather.h"
#include "game/sys/reverse_track.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Two in-memory user files stand in for the platform's storage: port.cfg is the one port_settings writes, and
 * extras.cfg is only read, as the fallback for the three settings that moved in from there. */
typedef struct {
    char name[32];
    uint8_t bytes[PORT_CFG_TEXT_MAX];
    size_t size;
    int present;
} Stored;

static Stored stored[2];

/* the slot holding `name`, or the first free one; a second file takes stored[1] rather than overwriting the first */
static inline Stored *slot_for(const char *name)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    strcpy(stored[0].name, name);
    return &stored[0];
}

static inline void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
    port_settings_forget();   /* the settings keep what they read */
}

static inline void clear_files(void)
{
    port_settings_forget();
    for (int i = 0; i < 2; i++) {
        stored[i].name[0] = '\0';
        stored[i].size = 0;
        stored[i].present = 0;
    }
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            uint8_t *copy = malloc(stored[i].size);
            memcpy(copy, stored[i].bytes, stored[i].size);
            *size = stored[i].size;
            return copy;
        }
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    Stored *s = slot_for(name);
    if (size > sizeof s->bytes)
        return false;
    memcpy(s->bytes, data, size);
    s->size = size;
    s->present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static inline void check_same(PortSettings a, PortSettings b)
{
    CHECK_EQ_INT(a.format, b.format);
    CHECK_EQ_INT(a.refresh_hz, b.refresh_hz);
    CHECK_EQ_INT(a.scale_integer, b.scale_integer);
    CHECK_EQ_INT(a.fullscreen, b.fullscreen);
    CHECK_EQ_INT(a.fov_adjust, b.fov_adjust);
    CHECK_EQ_INT(a.view_ultra, b.view_ultra);
    CHECK_EQ_INT(a.perspective, b.perspective);
    CHECK_EQ_INT(a.texture_filter, b.texture_filter);
    CHECK_EQ_INT(a.fog, b.fog);
    CHECK_EQ_INT(a.light_effects, b.light_effects);
    CHECK_EQ_INT(a.car_soft_shadow, b.car_soft_shadow);
    CHECK_EQ_INT(a.headlight_beams, b.headlight_beams);
    CHECK_EQ_INT(a.light_time, b.light_time);
    CHECK_EQ_INT(a.render_scale, b.render_scale);
    CHECK_EQ_INT(a.laps, b.laps);
    CHECK_EQ_INT(a.weather, b.weather);
    CHECK_EQ_INT(a.render_bands, b.render_bands);
    CHECK_EQ_INT(a.body_shine, b.body_shine);
    CHECK_EQ_INT(a.free_records, b.free_records);
    CHECK_EQ_INT(a.car_colours, b.car_colours);
    CHECK_EQ_INT(a.fog_sample, b.fog_sample);
    CHECK_EQ_INT(a.sky_fill, b.sky_fill);
    CHECK_EQ_INT(a.night_sky_hue, b.night_sky_hue);
    CHECK_EQ_INT(a.forest_dark, b.forest_dark);
    CHECK_EQ_INT(a.night_stretch_clean, b.night_stretch_clean);
    CHECK_EQ_INT(a.replay_no_ffb, b.replay_no_ffb);
    CHECK_EQ_INT(a.last_camera, b.last_camera);
    CHECK_EQ_INT(a.camera_kind, b.camera_kind);
    CHECK_EQ_INT(a.lamp_flares, b.lamp_flares);
    CHECK_EQ_INT(a.hall_light, b.hall_light);
    CHECK_EQ_INT(a.made_tunnels, b.made_tunnels);
    CHECK_EQ_INT(a.headlight_relight, b.headlight_relight);
    CHECK_EQ_INT(a.car_tunnel_light, b.car_tunnel_light);
    CHECK_EQ_INT(a.vegas_car_night, b.vegas_car_night);
    CHECK_EQ_INT(a.rally_gravel_shoulder, b.rally_gravel_shoulder);
    CHECK_EQ_INT(a.chase_height, b.chase_height);
    CHECK_EQ_INT(a.chase_steady, b.chase_steady);
    CHECK_EQ_INT(a.lamp_pool_tint, b.lamp_pool_tint);
    CHECK_EQ_INT(a.trans_shadows, b.trans_shadows);
    CHECK_EQ_INT(a.canyon_shade, b.canyon_shade);
    CHECK_EQ_INT(a.parking_dark, b.parking_dark);
    CHECK_EQ_INT(a.forest_neutral, b.forest_neutral);
    CHECK_EQ_INT(a.cavern_dark, b.cavern_dark);
    CHECK_EQ_INT(a.vegas_objects, b.vegas_objects);
    CHECK_EQ_INT(a.close_houses, b.close_houses);
    CHECK_EQ_INT(a.lamp_car_shade, b.lamp_car_shade);
    CHECK_EQ_INT(a.headlight_cone, b.headlight_cone);
    CHECK_EQ_INT(a.rain_fx, b.rain_fx);
    CHECK_EQ_INT(a.rain_grip, b.rain_grip);
    CHECK_EQ_INT(a.gauge_light, b.gauge_light);
    CHECK_EQ_INT(a.finish_gate_light, b.finish_gate_light);
    CHECK_EQ_INT(a.reverse_rows, b.reverse_rows);
    CHECK_EQ_INT(a.city_lift, b.city_lift);
    CHECK_EQ_INT(a.vegas_lift, b.vegas_lift);
    CHECK_EQ_INT(a.night_tone, b.night_tone);
    CHECK_EQ_INT(a.rain_lamps, b.rain_lamps);
    CHECK_EQ_INT(a.gauge_exterior, b.gauge_exterior);
    CHECK_EQ_INT(a.sign_glow, b.sign_glow);
    CHECK_EQ_INT(a.headlight_cone, b.headlight_cone);
    CHECK_EQ_INT(a.lamp_pool_bright, b.lamp_pool_bright);
    CHECK_EQ_INT(a.lamp_pools, b.lamp_pools);
    CHECK_EQ_INT(a.brake_over_gas, b.brake_over_gas);
    CHECK_EQ_INT(a.reflectors, b.reflectors);
    CHECK_EQ_INT(a.handbrake_lamps, b.handbrake_lamps);
    CHECK_EQ_INT(a.rally_spray, b.rally_spray);
    CHECK_EQ_INT(a.trail_rise, b.trail_rise);
    CHECK_EQ_INT(a.snow_fx, b.snow_fx);
    CHECK_EQ_INT(a.lamp_halos, b.lamp_halos);
    CHECK_EQ_INT(a.spray_cd_smoke, b.spray_cd_smoke);
    CHECK_EQ_INT(a.wipers, b.wipers);
    CHECK_EQ_INT(a.water_mist, b.water_mist);
    CHECK_EQ_INT(a.weather_fog, b.weather_fog);
    CHECK_EQ_INT(a.car_tunnel_match, b.car_tunnel_match);
    CHECK_EQ_INT(a.spray_v4, b.spray_v4);
    CHECK_EQ_INT(a.car_night_lift, b.car_night_lift);
    CHECK_EQ_INT(a.menu_pad_cancel, b.menu_pad_cancel);
    CHECK_EQ_INT(a.tunnel_lamps, b.tunnel_lamps);
}

#endif
