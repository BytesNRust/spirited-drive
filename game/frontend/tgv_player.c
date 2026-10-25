#include "game/frontend/tgv_player.h"

#include "game/frontend/menu_audio.h"
#include "game/frontend/tgv_frame.h"
#include "game/frontend/tgv_wide.h"
#include "game/sys/backdrop.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/video.h"
#include "game/viewer/drive_audio.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CHUNK_HEADER = 8,
    SCREEN_W = 320, SCREEN_H = 200,  /* 004423d4's clip (0047383c 0, 0x140, 0, 0xc8) */
    PER_SECOND = 15,                 /* 00441f18: the 100 Hz clock x 15 / 100 */
    SLACK = 8,                       /* the decoder reads a dword past a frame's codes */
    EACS_HEADER = 0x20, EACS_TYPE = 0xb, EACS_SAMPLES = 0xc, NO_TYPE = 0xff
};

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int tag_is(const uint8_t *p, const char *tag) { return memcmp(p, tag, 4) == 0; }

/* The file's chunks walked: the sound gathered ('1SNh' its EACS header and first bytes, then each '1SNd') and the
 * pictures counted; 0 when a chunk runs past the end */
static int walk(const uint8_t *file, size_t size, uint8_t **sound, size_t *sound_size)
{
    size_t total = 0;
    for (size_t at = 0; at + CHUNK_HEADER <= size;) {
        const uint32_t n = le32(file + at + 4);
        if (n < CHUNK_HEADER || n > size - at) return 0;
        if (tag_is(file + at, "1SNh") || tag_is(file + at, "1SNd")) total += n - CHUNK_HEADER;
        at += n;
    }
    *sound = total >= EACS_HEADER ? malloc(total) : NULL;
    *sound_size = 0;
    if (*sound == NULL) return 1;
    for (size_t at = 0; at + CHUNK_HEADER <= size; at += le32(file + at + 4)) {
        const uint32_t n = le32(file + at + 4);
        if (!tag_is(file + at, "1SNh") && !tag_is(file + at, "1SNd")) continue;
        memcpy(*sound + *sound_size, file + at + CHUNK_HEADER, n - CHUNK_HEADER);
        *sound_size += n - CHUNK_HEADER;
    }
    if (!tag_is(*sound, "EACS")) {
        free(*sound), *sound = NULL, *sound_size = 0;
        return 1;
    }
    /* the header's frames cut to the bytes the file holds (the last block may be short) */
    const uint32_t frame = (uint32_t)(*sound)[8] * (*sound)[9], frames = le32(*sound + EACS_SAMPLES);
    const uint32_t held = frame != 0 ? (uint32_t)((*sound_size - EACS_HEADER) / frame) : 0;
    const uint32_t keep = frames < held ? frames : held;
    for (int i = 0; i < 4; i++) (*sound)[EACS_SAMPLES + i] = (uint8_t)(keep >> (8 * i));
    /* the movies' type 0 would start the voice on channel 0 (00497784), away from the queue's channel that
     * menu_audio_movie_sound_stop stops: no type, so it plays where it is stopped */
    (*sound)[EACS_TYPE] = NO_TYPE;
    return 1;
}

/* The shape centred on the 320 x 200 screen as 004423d4 places it, drawn twice the size in the window */
static void draw(const uint8_t *shape)
{
    int ww, wh;
    uint8_t *out = video_target(&ww, &wh);
    const int w = shape[4] | shape[5] << 8, h = shape[6] | shape[7] << 8;
    const int x0 = ((SCREEN_W - w) / 2 + 3) & ~3, y0 = ((SCREEN_H - h) / 2 + 3) & ~3;
    const int top = (wh - 2 * SCREEN_H) / 2, left = (ww - 2 * SCREEN_W) / 2;
    memset(out, 0, (size_t)ww * (size_t)wh);
    for (int y = 0; y < SCREEN_H; y++) {
        const int sy = y - y0;
        if (sy < 0 || sy >= h) continue;
        uint8_t *row = out + (size_t)(top + 2 * y) * (size_t)ww + left;
        for (int x = 0; x < SCREEN_W; x++) {
            const int sx = x - x0;
            const uint8_t c = sx >= 0 && sx < w ? shape[TGV_SHAPE_HEADER + (size_t)sy * (size_t)w + (size_t)sx] : 0;
            row[2 * x] = row[2 * x + 1] = c;
        }
        memcpy(row + ww, row, 2 * SCREEN_W);
    }
}

/* 004424ac..004424e1: Space, Enter, Escape or a button */
static int stopped(void)
{
    const int key = input_get_key();
    return key == KEY_SPACE || key == KEY_ENTER || key == KEY_ESCAPE || pointer_buttons_unseen() != POINTER_BUTTON_NONE;
}

/* Waits for the picture's time; 1 when stopped meanwhile */
static int wait_until(uint32_t at)
{
    do {
        drive_audio_pump();
        if (stopped()) return 1;
    } while ((int32_t)(platform_ticks_ms() - at) < 0);
    return 0;
}

/* port (extras.cfg movies_wide): the width of a wide screen's frame, before the render scale, when the movies fill it;
 * 0 when they keep the exe's look (the 4:3 frame), as always at 4:3 */
static int wide_width(void)
{
    if (!port_extras_load().movies_wide) return 0;
    int width = VIDEO_WIDTH, height = VIDEO_HEIGHT;
    platform_framebuffer_size(&width, &height);
    const int scale = height >= VIDEO_HEIGHT ? height / VIDEO_HEIGHT : 1;
    return width / scale > VIDEO_WIDTH ? width / scale : 0;
}

/* port: the dark columns at a movie's edges (tgv_wide.h), from a pass over all its pictures with a state of its own */
static void scan_edges(const uint8_t *file, size_t size, int *left, int *right)
{
    TgvEdges edges = { 0 };
    TgvState *scan = tgv_open();
    for (size_t at = 0; scan != NULL && at + CHUNK_HEADER <= size; at += le32(file + at + 4)) {
        if (!tag_is(file + at, "kVGT") && !tag_is(file + at, "TGVk") && !tag_is(file + at, "fVGT") && !tag_is(file + at, "TGVf")) continue;
        const uint8_t *shape = tgv_decode(scan, file + at, le32(file + at + 4));
        if (shape != NULL) tgv_edges_add(&edges, shape + TGV_SHAPE_HEADER, shape[4] | shape[5] << 8, shape[6] | shape[7] << 8, scan->palette);
    }
    tgv_close(scan);
    tgv_edges_trim(&edges, left, right);
}

static int play(const uint8_t *file, size_t size, TgvState *s)
{
    const uint32_t start = platform_ticks_ms();
    uint32_t pictures = 0;
    const int wide = wide_width();
    uint8_t *picture = wide != 0 ? malloc((size_t)wide * VIDEO_HEIGHT) : NULL;
    uint8_t shade[256] = { 0 };
    int trim_left = 0, trim_right = 0;
    if (picture != NULL) scan_edges(file, size, &trim_left, &trim_right);
    for (size_t at = 0; at + CHUNK_HEADER <= size; at += le32(file + at + 4)) {
        const int key = tag_is(file + at, "kVGT") || tag_is(file + at, "TGVk");
        if (!key && !tag_is(file + at, "fVGT") && !tag_is(file + at, "TGVf")) continue;
        const uint8_t *shape = tgv_decode(s, file + at, le32(file + at + 4));
        if (wait_until(start + pictures * 1000u / PER_SECOND)) {
            video_set_wide_picture(NULL, 0);
            free(picture);
            return 1;
        }
        pictures++;
        if (shape == NULL) continue;
        draw(shape);   /* the frame as before: what the screen is left with after the movie */
        if (picture != NULL) {
            if (key) backdrop_shade_table(s->palette, shade);
            tgv_wide_compose(shape + TGV_SHAPE_HEADER, shape[4] | shape[5] << 8, shape[6] | shape[7] << 8, picture, wide, VIDEO_HEIGHT, shade, trim_left, trim_right);
            video_set_wide_picture(picture, wide);
        }
        video_flip();
        if (key) video_set_palette(0, 256, s->palette);  /* 004424ac: after the flip, the key frame's colours */
    }
    video_set_wide_picture(NULL, 0);
    free(picture);
    return 0;
}

/* 004423d4 */
int tgv_player_play(const char *path)
{
    size_t size = 0;
    uint8_t *data = resource_load(path, &size);  /* any case, QFS unpacked */
    if (data == NULL) {
        port_log("tgv_player: no movie %s\n", path);
        return 0;
    }
    uint8_t *file = malloc(size + SLACK);
    TgvState *s = tgv_open();
    uint8_t *sound = NULL;
    size_t sound_size = 0;
    int skipped = 0;
    if (file != NULL && s != NULL) {
        memcpy(file, data, size), memset(file + size, 0, SLACK);
        if (walk(file, size, &sound, &sound_size)) {
            if (sound != NULL) menu_audio_movie_sound(sound, sound_size);
            skipped = play(file, size, s);
            if (sound != NULL) menu_audio_movie_sound_stop();
        } else {
            port_log("tgv_player: %s has a chunk past its end\n", path);
        }
    }
    free(sound), tgv_close(s), free(file), resource_free(data);
    return skipped;
}
