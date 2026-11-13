/* The settings accessors for the effects: the track lights, the sky and the cars' and weather's effects (port_settings.h; split from port_settings.c by responsibility). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/race/session_rules.h"
#include "game/race/weather.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/viewer/screen_layout.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/sys/port_settings_private.h"

int port_analog_pad(void)
{
    const char *v = getenv("TNFS_ANALOG_PAD");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().analog_pad;
}

int port_sign_glow(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_SIGN_GLOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().sign_glow;
}

int port_gauge_exterior(void)
{
    const char *v = getenv("TNFS_GAUGE_EXTERIOR");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_exterior;
}

int port_city_lift(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_CITY_LIFT");
    if (v != NULL && v[0] >= '0' && v[0] <= '3' && v[1] == '\0') return v[0] != '0';   /* a switch: an older higher step is 1 */
    return port_settings_load().city_lift;
}

int port_vegas_lift(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_VEGAS_LIFT");
    if (v != NULL && v[0] >= '0' && v[0] <= '2' && v[1] == '\0') return v[0] != '0';   /* a switch: an older higher step is 1 */
    return port_settings_load().vegas_lift;
}

int port_replay_no_ffb(void)
{
    const char *v = getenv("TNFS_REPLAY_NO_FFB");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().replay_no_ffb;
}

int port_last_camera(void)
{
    const char *v = getenv("TNFS_LAST_CAMERA");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().last_camera;
}

int sky_fill_below_applies(const PortSettings *s) { return s->sky_fill_below != 0 && s->format != SCREEN_4_3; }

int port_sky_fill_below(void)
{
    PortSettings s = port_settings_load();
    const char *v = getenv("TNFS_SKY_FILL_BELOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') s.sky_fill_below = v[0] - '0';
    return sky_fill_below_applies(&s);
}

int port_host_allows_radar_and_camera(void)
{
    const SessionRules *host = session_rules_active();
    return host == NULL || host->mode == SESSION_RESTOMOD;
}

int port_radar_all_views(void)
{
    const char *v = getenv("TNFS_RADAR_ALL_VIEWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().radar_all_views;
}

int port_tach_dim(void)
{
    const char *v = getenv("TNFS_TACH_DIM");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().tach_dim;
}

int port_gauge_wheel(void)
{
    const char *v = getenv("TNFS_GAUGE_WHEEL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_wheel;
}

int port_police_glow(void)
{
    const char *v = getenv("TNFS_POLICE_GLOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().police_glow;
}

int port_sky_fill(void)
{
    const char *v = getenv("TNFS_SKY_FILL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().sky_fill;
}

int port_sky_fill_mode(void)
{
    const char *v = getenv("TNFS_SKY_FILL_MODE");
    if (v != NULL && (strcmp(v, "a") == 0 || strcmp(v, "1") == 0)) return 1;
    if (v != NULL && (strcmp(v, "b") == 0 || strcmp(v, "2") == 0)) return 2;
    return SKY_FILL_MODE_DEFAULT;
}

int port_free_records(void)
{
    const char *v = getenv("TNFS_FREE_RECORDS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().free_records;
}

int port_car_colours(void)
{
    const char *v = getenv("TNFS_CAR_COLOURS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_colours;
}

int port_headlight_beams(void)
{
    const char *v = port_env_alias("TNFS_HEADLIGHT_BEAMS", "TNFS_PSX_HEADLIGHTS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().headlight_beams;
}

int port_car_soft_shadow(void)
{
    const char *v = port_env_alias("TNFS_CAR_SOFT_SHADOW", "TNFS_PSX_SHADOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_soft_shadow;
}

int port_emissive_lamps(void)
{
    const char *v = getenv("TNFS_EMISSIVE_LAMPS");   /* a dev override; the game follows the Graphics Light Effects switch */
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_light_effects();
}

int port_body_shine(void)
{
    const char *v = getenv("TNFS_BODY_SHINE");
    if (v != NULL && v[0] >= '0' && v[0] <= '4' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().body_shine;
}

int port_light_effects(void)
{
    const char *v = port_env_alias("TNFS_LIGHT_EFFECTS", "TNFS_PSX_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().light_effects;
}

int port_rain_tyre_roll(void)
{
    const char *v = getenv("TNFS_RAIN_TYRE_ROLL");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_tyre_roll;
}

int port_snow_weather(void)
{
    const char *v = getenv("TNFS_SNOW_WEATHER");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    const SessionRules *host = session_rules_active();   /* port (online): the host's snow: it changes the grip */
    return host != NULL ? host->snow_weather : port_settings_load().snow_weather;
}

int port_handbrake_lamps(void)
{
    const char *v = getenv("TNFS_HANDBRAKE_LAMPS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().handbrake_lamps;
}

int port_reflectors(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_REFLECTORS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().reflectors;
}

int port_rally_spray(void)
{
    const char *v = getenv("TNFS_RALLY_SPRAY");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rally_spray;
}

int port_wipers(void)
{
    const char *v = getenv("TNFS_WIPERS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().wipers;
}

int port_spray_v4(void)
{
    const char *v = getenv("TNFS_SPRAY_V4");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().spray_v4;
}

int port_snow_fx(void)
{
    const char *v = getenv("TNFS_SNOW_FX");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().snow_fx;
}

int port_lamp_halos(void)
{
    const char *v = getenv("TNFS_LAMP_HALOS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_halos;
}

int port_weather_fog(void)
{
    const char *v = getenv("TNFS_WEATHER_FOG");
    if (v != NULL && v[0] >= '0' && v[0] <= '2' && v[1] == '\0') return v[0] != '0';   /* a switch: an older higher step is 1 */
    return port_settings_load().weather_fog;
}

int port_menu_pad_cancel(void)
{
    const char *v = getenv("TNFS_MENU_PAD_CANCEL");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().menu_pad_cancel;
}

int port_light_edit(void)
{
    const char *v = getenv("TNFS_LIGHT_EDIT");
    if (v != NULL && v[0] != '\0') return v[0] != '0';
    return port_settings_load().light_edit;
}

int port_tunnel_lamps(void)
{
    const char *v = getenv("TNFS_TUNNEL_LAMPS");
    if (v != NULL && v[0] >= '0' && v[0] <= '2' && v[1] == '\0') return v[0] != '0';   /* a switch: an older higher step is 1 */
    return port_settings_load().tunnel_lamps;
}

int port_wrong_way(void)
{
    const char *v = getenv("TNFS_WRONG_WAY");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().wrong_way;
}
