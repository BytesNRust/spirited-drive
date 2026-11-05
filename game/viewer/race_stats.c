/* port: the race's stats overlay's figures and switch (race_stats.h): the F12 toggle, the frame meter, the words. The
 * drawing is race_stats_draw.c's. */
#include "viewer/race_stats.h"

#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"

#include <stdio.h>

void race_stats_start(RaceStats *s, int on)
{
    *s = (RaceStats){ .on = on != 0 };
}

int race_stats_key(RaceStats *s, int down)
{
    const int press = down != 0 && !s->key_down;
    s->key_down = down != 0;
    if (!press) return 0;
    s->on = !s->on;
    return 1;
}

int race_stats_key_save(RaceStats *s, int down)
{
    if (!port_profile_key_free((Profile)port_settings_profile(), "stats_overlay")) return 0;   /* port (modes): no effect where the mode holds it */
    if (!race_stats_key(s, down)) return 0;
    port_extras_set_race_switch(RACE_SWITCH_STATS, s->on);
    return 1;
}

void race_stats_meter_frame(RaceStatsMeter *m, uint32_t now_ms)
{
    if (!m->primed) {
        *m = (RaceStatsMeter){ .primed = 1, .window_ms = now_ms, .last_ms = now_ms, .figures = m->figures };
        return;
    }
    const uint32_t gap = now_ms >= m->last_ms ? now_ms - m->last_ms : 0;
    m->frames++;
    m->sum_ms += gap;
    if (gap > m->max_ms) m->max_ms = gap;
    m->last_ms = now_ms;
    const uint32_t elapsed = now_ms >= m->window_ms ? now_ms - m->window_ms : 0;
    if (elapsed < RACE_STATS_WINDOW_MS) return;
    const double seconds_ms = (double)elapsed;
    m->figures.ready = 1;
    m->figures.fps = m->frames * 1000.0 / seconds_ms;
    m->figures.frame_avg_ms = m->frames > 0 ? (double)m->sum_ms / m->frames : 0.0;
    m->figures.frame_max_ms = m->max_ms;
    *m = (RaceStatsMeter){ .primed = 1, .window_ms = now_ms, .last_ms = now_ms, .figures = m->figures };
}

void race_stats_sample(RaceStats *s, uint32_t now_ms)
{
    race_stats_meter_frame(&s->meter, now_ms);
}

/* the render's ratio from its size: 4:3 below 1.6, 16:9 below 2.0, else 21:9 (the widths port.cfg's formats make) */
static const char *ratio_name(int width, int height)
{
    enum { RATIO_4_3 = 160, RATIO_16_9 = 200 };  /* width * 100 / height */
    if (height <= 0) return "";
    const int ratio = width * 100 / height;
    return ratio < RATIO_4_3 ? "4:3" : ratio < RATIO_16_9 ? "16:9" : "21:9";
}

void race_stats_format(const RaceStatsView *v, RaceStatsText *out)
{
    const RaceStatsFigures *f = &v->figures;
    if (f->ready)
        /* no comma: MAIN18.FFN draws it as another glyph */
        snprintf(out->line[0], RACE_STATS_LINE_LEN, "FPS %.1f  frame %.1f ms  max %u ms", f->fps, f->frame_avg_ms,
                 (unsigned)f->frame_max_ms);
    else
        snprintf(out->line[0], RACE_STATS_LINE_LEN, "FPS --  frame -- ms");
    snprintf(out->line[1], RACE_STATS_LINE_LEN, "Refresh %d Hz  interp %s", v->refresh_hz, v->interp ? "on" : "off");
    snprintf(out->line[2], RACE_STATS_LINE_LEN, "Render %dx%d  %s", v->width, v->height,
             ratio_name(v->width, v->height));
    snprintf(out->line[3], RACE_STATS_LINE_LEN, "Perspective correction %s  filter %s  fog %s", v->modern ? "on" : "off",
             v->filter ? "on" : "off", v->fog ? "on" : "off");
    snprintf(out->line[4], RACE_STATS_LINE_LEN, "View %d segments", v->segments);
}
