/* --drive's headless screenshot run (drive_screenshot): the ticks stepped with the captures' env switches, the frames timed (TNFS_FRAME_TIMES) and the picture written. Split from drive_run.c. */
#include "game/sys/port_log.h"
#include "viewer/drive_autopilot.h"
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "viewer/drive_light.h"
#include "game/gfx/blit.h"
#include "game/sys/fixed_math.h"
#include "game/race/ai_lanes.h"
#include "game/race/car_draw_setup.h"
#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay.h"
#include "game/race/start_keys.h"
#include "viewer/drive_audio.h"
#include "viewer/drive_clock.h"
#include "viewer/drive_interp.h"
#include "viewer/race_stats.h"
#include "viewer/light_edit.h"
#include "viewer/spray_v4_draw.h"
#include "viewer/view_spray.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/race/race_follow.h"
#include "game/sys/port_controls.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "viewer/drive_run_private.h"

enum { SPIN_TICKS = 150, SPIN_BRAKE_TICKS = 30 };   /* TNFS_SCRIPT=spin: the last 150 ticks, the handbrake the first 30 */

/* The screenshots' input on step i of `ticks`: full throttle, a tap of
 * 0x1000 while the race tick is 320..327, and over the last 20 steps the
 * lock TNFS_STEER=left / right asks for. */
uint32_t screenshot_input(uint32_t i, uint32_t ticks)
{
    const Car *own = drive_sim.race.cars[drive_sim.race.player];
    if (drive_autopilot_on() && own != NULL)   /* port: test harness only (drive_autopilot.h): the headless run is driven as the window run is */
        return drive_autopilot_word(own, &drive_sim.race, drive_input.own_specs);
    const char *script = getenv("TNFS_SCRIPT");
    if (script != NULL && strcmp(script, "spin") == 0 && i + SPIN_TICKS >= ticks) {   /* full lock left, the handbrake for a second */
        const uint32_t into = i + SPIN_TICKS - ticks;
        return (into < SPIN_BRAKE_TICKS ? 0x8000u : 0u) | 0u;
    }
    if (script != NULL && strcmp(script, "brake") == 0 && i + 40 >= ticks) return 0x20u | (0x3fu << 6);   /* TNFS_SCRIPT=brake: the brake held over the last 40 steps (the lamps' captures) */
    const char *steer_env = getenv("TNFS_STEER");
    const uint32_t lock = steer_env == NULL ? 0x20 : strcmp(steer_env, "left") == 0 ? 0 : 0x3f;
    const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
    return (i + 20 >= ticks ? lock : 0x20) | (tap ? 0x1000u : 0);
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot (taken at the run's end, where the car has driven to: TNFS_START_NODE is not the frame's node; the eye node of the last frame is on the "tick" line, and of each TNFS_SHOTS shot on its line)
 * (TNFS_VIEW=N: from view kind N; TNFS_STEER=left / right: full lock at the
 * end; TNFS_NOTICE=1 / 2: that police stop notice up for the last 10 ticks,
 * TNFS_NOTICE=1off / 2off: and down again for the last 5; TNFS_FRAMES=N:
 * whole frames over the last N ticks, so what builds up while drawing, the
 * skid marks, does). */
/* TNFS_SHOTS=prefix:every: a picture (prefix_tick.ppm) every `every` ticks of the last frames, with the eye's node and the car's and the camera's yaw
 * (the follow camera's tests). The eye node in the line is the frame's: TNFS_START_NODE only says where the car starts, and the car drives on at full throttle. */
static void spin_shot(const Track *t, uint8_t *pixels, uint32_t i, uint32_t ticks)
{
    const char *spec = getenv("TNFS_SHOTS");
    const char *colon = spec != NULL ? strrchr(spec, ':') : NULL;
    if (colon == NULL || atoi(colon + 1) <= 0 || i % (uint32_t)atoi(colon + 1) != 0) return;
    const Car *car = drive_sim.race.cars[0];
    int32_t off = (int32_t)(((uint32_t)drive_sim.race.chase.angles[1] -
                             (uint32_t)race_node(&drive_sim.race, drive_sim.race.chase.node).orientation * 0x400u) & 0xffffff);
    if (off > 0x800000) off -= 0x1000000;
    printf("shot %u: eye node %d, car yaw %d, camera yaw %d (a turn is 16777216), camera %.0f degrees off the road, speed %.1f\n", i, (int)drive_sim.race.chase.node,
           car->yaw & 0xffffff, drive_sim.race.chase.angles[1] & 0xffffff, off * 360.0 / 16777216.0, car->speed / 65536.0);
    char name[512];
    snprintf(name, sizeof name, "%.*s_%05u.ppm", (int)(colon - spec), spec, i);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    write_drive_ppm(name, shown, width, t->art.palette);   /* as the final screenshot and the window: the time of day's tint too */
    (void)ticks;
}

/* TNFS_FRAME_TIMES=1: the whole frames' render times (the follow camera's cost) */
enum { FRAME_TIMES = 4096 };
static double frame_us[FRAME_TIMES], present_us[FRAME_TIMES];   /* the render's, and the present's (present_dry) */
static int frame_count;
static double spray_sum[6];   /* the frames' live, puffs, big, strips, texels (spray v4) */
static long spray_max_texels;
static int spray_frames;

static int compare_us(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void frame_times_print(void)
{
    if (getenv("TNFS_FRAME_TIMES") == NULL || frame_count == 0) return;
    double total[FRAME_TIMES], sum = 0, psum = 0, tsum = 0;
    for (int i = 0; i < frame_count; i++) sum += frame_us[i], psum += present_us[i], tsum += (total[i] = frame_us[i] + present_us[i]);
    qsort(frame_us, (size_t)frame_count, sizeof frame_us[0], compare_us);
    printf("frames %d: mean %.0f us, p50 %.0f us, p95 %.0f us, max %.0f us\n", frame_count, sum / frame_count,
           frame_us[frame_count / 2], frame_us[(int)(frame_count * 0.95)], frame_us[frame_count - 1]);
    qsort(present_us, (size_t)frame_count, sizeof present_us[0], compare_us);
    qsort(total, (size_t)frame_count, sizeof total[0], compare_us);
    printf("present %d: mean %.0f us, p50 %.0f us, p95 %.0f us, max %.0f us (no upload)\n", frame_count, psum / frame_count,
           present_us[frame_count / 2], present_us[(int)(frame_count * 0.95)], present_us[frame_count - 1]);
    printf("total %d: mean %.0f us, p50 %.0f us, p95 %.0f us, max %.0f us\n", frame_count, tsum / frame_count,
           total[frame_count / 2], total[(int)(frame_count * 0.95)], total[frame_count - 1]);
    if (spray_frames > 0 && spray_sum[0] + spray_sum[1] > 0)   /* spray v4 on: what each frame drew, so a cost regression shows in the log */
        printf("spray v4 per frame (mean of %d): live %.0f, puffs %.1f (big %.1f), strips %.1f, texels %.0f (max %ld), cells %.0f\n", spray_frames,
               spray_sum[0] / spray_frames, spray_sum[1] / spray_frames, spray_sum[2] / spray_frames, spray_sum[3] / spray_frames,
               spray_sum[4] / spray_frames, spray_max_texels, spray_sum[5] / spray_frames);
}

static void spray_stats_add(void)
{
    const SprayV4Stats *s = spray_v4_stats();
    spray_sum[0] += s->live, spray_sum[1] += s->puffs, spray_sum[2] += s->big, spray_sum[3] += s->strips, spray_sum[4] += (double)s->texels, spray_sum[5] += (double)s->cells;
    if (s->texels > spray_max_texels) spray_max_texels = s->texels;
    spray_frames++;
}

int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    drive_wide_size(screen_width(drive_screen_format()));
    start_at_node(&drive_sim.race);   /* port (dev, env only): TNFS_START_NODE, as the window run takes it */
    view_spray_v4_hook();   /* port (spray v4): the trail from the first tick, so a short TNFS_FRAMES has the whole trail the window would have */
    const char *view = getenv("TNFS_VIEW");
    const char *follow = getenv("TNFS_FOLLOW_CAR");   /* port (dev, env only): the chase view follows this slot's car (2: the police car) */
    Car *const followed = follow != NULL && atoi(follow) > 0 && atoi(follow) < RACE_CARS && drive_sim.race.cars[atoi(follow)] != NULL
                              ? drive_sim.race.cars[atoi(follow)] : drive_sim.race.cars[0];
    if (view != NULL) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, followed, atoi(view) % CHASE_KINDS);
    const char *frames_env = getenv("TNFS_FRAMES");
    const uint32_t frames = frames_env == NULL ? 0 : (uint32_t)atoi(frames_env);
    const int f3_keys = getenv("TNFS_F3_AT") != NULL;  /* the captures' F3 presses: the race's keys, as the window run reads them */
    /* port (dev, env only): TNFS_LOOK_BACK_AT=from:to holds the look back key (B, or the player's) over those ticks, for the captures */
    int32_t look_from = -1, look_to = -1;
    const char *look_env = getenv("TNFS_LOOK_BACK_AT");
    if (look_env != NULL) sscanf(look_env, "%d:%d", &look_from, &look_to);
    const PortControls look_port = port_controls_load();
    /* port (dev, env only): TNFS_VIEW_AT=tick:kind takes the chase camera to that kind at that tick, as a view key would (the cameras' change, for the captures) */
    int32_t view_at = -1, view_to = 0;
    const char *view_at_env = getenv("TNFS_VIEW_AT");
    if (view_at_env != NULL) sscanf(view_at_env, "%d:%d", &view_at, &view_to);
    /* port (dev, env only): TNFS_FRAME_SKIP=n draws one tick in n of the last TNFS_FRAMES (the last one always), as a slow machine does: the ticks between have no frame (the trails must lay them, spray and dust) */
    const char *skip_env = getenv("TNFS_FRAME_SKIP");
    const uint32_t frame_skip = skip_env != NULL && atoi(skip_env) > 1 ? (uint32_t)atoi(skip_env) : 1;
    const char *dump_path = getenv("TNFS_AUDIO_DUMP");   /* port (dev, env only): the race's sound mixed offline to this WAV (drive_audio.c) */
    const int dumping = dump_path != NULL && drive_audio_offline_open(&drive_sim.race, drive_cars.data_root, dump_path);
    if (dumping) {
        race_sound_setup(&drive_sim.race, run_music_volume, NULL);
        race_sound_load_banks(&drive_sim.race, drive_audio_load, &drive_banks);
        race_sound_start_voices(&drive_sim.race);
    }
    for (uint32_t i = 0; i < ticks; i++) {
        drive_replay_key(&drive_replay, env_press((int32_t)i));
        if (view_at >= 0 && (int32_t)i == view_at) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, followed, view_to % CHASE_KINDS);
        if (look_from >= 0) {
            const int32_t key = port_controls_look_back_key(&look_port);
            drive_input.keys[key] = (int32_t)i >= look_from && (int32_t)i < look_to;
            drive_sim.race.chase.port_look_back = race_look_back_held(drive_sim.race.port_follow_camera, drive_input.keys, (int32_t)sizeof drive_input.keys, key);
            race_look_back_view(&drive_sim.race);
        }
        if (f3_keys) {
            drive_input.keys[KEY_F3] = f3_env_press((int32_t)i) ? 1 : 0;
            drive_keys_frame(&drive_keys, &drive_input, &drive_sim.race, &drive_views);
            drive_input.keys[KEY_F3] = 0;
            take_detail(&drive_keys.detail_pending);
        }
        drive_tick(screenshot_input(i, ticks));
        police_keep(&drive_sim.race);
        if (dumping) drive_audio_offline_tick();
        const char *notice = getenv("TNFS_NOTICE");
        if (notice != NULL && (notice[0] == '1' || notice[0] == '2') && i + 10 >= ticks) {
            drive_sim.race.stop_notice[notice[0] - '1'] = !(notice[1] == 'o' && i + 5 >= ticks);
            render_drive(t, pixels);  /* whole frames, so the screen comes and goes as on screen */
            continue;
        }
        if (i + frames >= ticks && (ticks - 1 - i) % frame_skip == 0) {
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            render_drive(t, pixels);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            struct timespec t2 = t1;
            if (getenv("TNFS_FRAME_TIMES") != NULL) {   /* dev: and the present's work with no upload (present_dry), timed on its own */
                present_dry = 1;
                present_drive(t, pixels);
                present_dry = 0;
                clock_gettime(CLOCK_MONOTONIC, &t2);
            }
            if (frame_count < FRAME_TIMES) spray_stats_add();
            if (frame_count < FRAME_TIMES) {
                present_us[frame_count] = (t2.tv_sec - t1.tv_sec) * 1000000.0 + (t2.tv_nsec - t1.tv_nsec) / 1000.0;
                frame_us[frame_count++] = (t1.tv_sec - t0.tv_sec) * 1000000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000.0;
            }
            spin_shot(t, pixels, i, ticks);
            continue;
        }
        drive_hud_strip(&drive_hud, &drive_sim.race);  /* the HUD keeps drawing every frame */
        ported_view_stream(&t->tri, &t->art, chase_camera(&drive_sim.race.chase, t->tri.ring_nodes).node);  /* and 0043f540 */
    }
    if (dumping) drive_audio_offline_close();
    frame_times_print();
    const Car *car = drive_sim.race.cars[0];
    const RaceChase *chase = &drive_sim.race.chase;
    printf("tick %d (phase %d): node %d, speed %.1f m/s, gear %d; view kind %d, eye node %d (%.1f, %.1f, %.1f)\n",
           drive_sim.race.tick, drive_sim.race.phase, car->node, car->speed / 65536.0, car->gear, chase->kind, chase->node,
           chase->position[0] / 65536.0, chase->position[1] / 65536.0, chase->position[2] / 65536.0);
    render_drive(t, pixels);
    if (skip_env != NULL) {   /* port (dev, env only): the trails' continuity, for the frame-skip test */
        int rain_ticks, dust_ticks;
        ported_view_trail_stats(&rain_ticks, &dust_ticks);
        printf("trail ticks: rain %d dust %d\n", rain_ticks, dust_ticks);
    }
    printf("segments %d\n", (int)ported_view_draw_segments());  /* the F12 overlay's "View n segments" (drive_run.c) */
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    return write_drive_ppm(screenshot, shown, width, t->art.palette);
}
