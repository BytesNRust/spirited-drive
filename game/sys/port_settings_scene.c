/* The settings accessors for the scene: the screen, the weather and time, the fog and the lamps (port_settings.h; split from port_settings.c by responsibility). */
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

PlatformConfig port_platform_config(const char *title)
{
    const PortSettings s = port_settings_load();
    return (PlatformConfig){ screen_width(s.format), SCREEN_HEIGHT, title, s.scale_integer, s.fullscreen, s.refresh_hz };
}

static int conditions_on, conditions_time, conditions_weather;

void port_conditions_force(int on, int light_time, int weather)
{
    conditions_on = on != 0, conditions_time = light_time, conditions_weather = weather;
}

int port_weather(void)
{
    const char *v = getenv("TNFS_WEATHER");
    const int w = v != NULL ? weather_named(v) : -1;
    if (w >= 0) return w;
    if (conditions_on) return conditions_weather;   /* port (replay conditions): a replay's own */
    const SessionRules *host = session_rules_active();   /* port (online): a link race runs the host's weather */
    return host != NULL ? host->weather : port_settings_load().weather;
}

int port_render_scale(void)
{
    const char *v = getenv("TNFS_RENDER_SCALE");
    if (v != NULL && v[0] >= '1' && v[0] <= '3' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().render_scale;
}

int port_light_time(void)
{
    const char *v = port_env_alias("TNFS_LIGHT_TIME", "TNFS_PSX_TIME");
    const int t = v != NULL ? port_settings_light_time_named(v) : -1;
    const SessionRules *host = session_rules_active();   /* port (online): the host's time of day */
    return t >= 0 ? t : conditions_on ? conditions_time : host != NULL ? host->light_time : port_settings_load().light_time;
}

int port_fog_sample(void)
{
    const char *v = getenv("TNFS_FOG_SAMPLE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().fog_sample;
}

int port_fog_lift(void)
{
    const char *v = getenv("TNFS_FOG_LIFT");
    char *end;
    const long n = v != NULL ? strtol(v, &end, 10) : -1;
    return v != NULL && end != v && *end == '\0' && n >= 0 && n <= 100 ? (int)n : FOG_SAMPLE_LIFT_DEFAULT;
}

int port_two_sided_scenery(int automatic)
{
    const char *v = getenv("TNFS_TWO_SIDED");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_close_boxes(int automatic)
{
    const char *v = getenv("TNFS_CLOSE_BOXES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_made_tunnels(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_MADE_TUNNELS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().made_tunnels;
}

int port_lamp_flares(void)
{
    if (!port_light_effects()) return 0;   /* the halo is the light effects' */
    const char *v = port_env_alias("TNFS_LAMP_FLARES", "TNFS_PSX_FLARES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_flares;
}

int port_car_tunnel_light(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_CAR_TUNNEL_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_tunnel_light;
}

int port_vegas_car_night(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_VEGAS_CAR_NIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().vegas_car_night;
}

int port_rally_gravel_shoulder(void)
{
    const char *v = getenv("TNFS_RALLY_GRAVEL_SHOULDER");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rally_gravel_shoulder;
}

int port_chase_height(void)
{
    const char *v = getenv("TNFS_CHASE_HEIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().chase_height;
}

int port_chase_steady(void)
{
    const char *v = getenv("TNFS_CHASE_STEADY");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().chase_steady;
}

int port_lamp_pool_tint(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_LAMP_POOL_TINT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pool_tint;
}

int port_trans_shadows(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_TRANS_SHADOWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().trans_shadows;
}

int port_canyon_shade(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_CANYON_SHADE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().canyon_shade;
}

int port_parking_dark(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_PARKING_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().parking_dark;
}

int port_forest_neutral(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_FOREST_NEUTRAL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().forest_neutral;
}

int port_cavern_dark(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_CAVERN_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().cavern_dark;
}

int port_vegas_objects(void)
{
    if (!port_light_effects()) return 0;
    const char *v = getenv("TNFS_VEGAS_OBJECTS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().vegas_objects;
}

int port_close_houses(void)
{
    const char *v = getenv("TNFS_CLOSE_HOUSES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().close_houses;
}

int port_brake_over_gas(void)
{
    const char *v = getenv("TNFS_BRAKE_OVER_GAS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().brake_over_gas;
}

int port_lamp_pools(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_LAMP_POOLS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pools;
}

int port_lamp_car_shade(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_LAMP_CAR_SHADE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_car_shade;
}

int port_rain_grip_active(void)
{
    return port_rain_grip() && weather_wet(port_weather());
}

int port_gauge_light(void)
{
    const char *v = getenv("TNFS_GAUGE_LIGHT");
    if (v != NULL && v[0] >= '0' && v[0] <= '5' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_light;
}

int port_finish_gate_light(void)
{
    const char *v = getenv("TNFS_FINISH_GATE_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().finish_gate_light;
}

int port_rain_grip(void)
{
    const char *v = getenv("TNFS_RAIN_GRIP");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    const SessionRules *host = session_rules_active();   /* port (online): the host's grip rule */
    return host != NULL ? host->rain_grip : port_settings_load().rain_grip;
}

int port_rain_fx(void)
{
    const char *v = getenv("TNFS_RAIN_FX");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_fx;
}

/* Perspective Correction on: the cone is drawn on Modern 3D's road (TNFS_MODERN_3D overrides the setting) */
static int perspective_on(void)
{
    const char *v = getenv("TNFS_MODERN_3D");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().perspective;
}

int port_lamp_pool_bright(void)
{
    if (!port_headlight_beams()) return 0;
    const char *v = getenv("TNFS_LAMP_POOL_BRIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pool_bright;
}

int port_headlight_cone(void)
{
    if (!port_headlight_beams() || !perspective_on()) return 0;   /* the cone needs perspective (Modern 3D): without it the node pool, as the exe's */
    const char *v = getenv("TNFS_HEADLIGHT_CONE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().headlight_cone;
}

int port_reverse_rows(void)
{
    const char *v = getenv("TNFS_REVERSE_ROWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().reverse_rows;
}

int port_rain_lamps(void)
{
    const char *v = getenv("TNFS_RAIN_LAMPS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_lamps;
}

int port_rally_dry_lamps(void)
{
    const char *v = getenv("TNFS_RALLY_DRY_LAMPS");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rally_dry_lamps;
}

int port_traffic_halo(void)
{
    const char *v = getenv("TNFS_TRAFFIC_HALO");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().traffic_halo;
}

int port_rain_v2(void)
{
    const char *v = getenv("TNFS_RAIN_V2");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_v2;
}

int port_shoulder_light(void)
{
    const char *v = getenv("TNFS_SHOULDER_LIGHT");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().shoulder_light;
}
