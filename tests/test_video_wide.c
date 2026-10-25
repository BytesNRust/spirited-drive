/* port: a wide picture set with video_set_wide_picture is what the screen shows at a flip and at a palette change, in
 * place of the menu frame over its backdrop; cleared, or of another width, the frame shows as before. SDL's dummy video
 * driver; what was presented is read from the platform's capture file. */
#include "game/sys/video.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDE = 854 };

static uint8_t picture[WIDE * VIDEO_HEIGHT];
static uint8_t seen[3 * WIDE * VIDEO_HEIGHT];

/* the last presented frame's RGB, from the capture directory; 0 when there is none */
static int last_capture(const char *dir, unsigned frame)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/frame_%05u.ppm", dir, frame);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    int w = 0, h = 0, max = 0;
    const int ok = fscanf(f, "P6 %d %d %d", &w, &h, &max) == 3 && fgetc(f) == '\n' && w == WIDE && h == VIDEO_HEIGHT &&
                   fread(seen, 1, sizeof seen, f) == sizeof seen;
    fclose(f);
    return ok;
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    const char *dir = ".";   /* the test's working directory: frame_00000.ppm .. frame_00004.ppm, removed at the end */
    setenv("TNFS_CAPTURE_DIR", dir, 1);
    setenv("TNFS_CAPTURE_EVERY", "1", 1);
    const PlatformConfig config = { WIDE, VIDEO_HEIGHT, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    CHECK(video_init());
    uint8_t palette[768];
    for (int i = 0; i < 256; i++) palette[3 * i] = (uint8_t)i, palette[3 * i + 1] = (uint8_t)(255 - i), palette[3 * i + 2] = 7;
    video_set_palette(0, 256, palette);   /* presents: capture frame 0 */
    for (int i = 0; i < WIDE * VIDEO_HEIGHT; i++) picture[i] = (uint8_t)(i * 31 % 251);

    video_set_wide_picture(picture, WIDE);
    video_flip();   /* capture frame 1: the wide picture, whole */
    CHECK(last_capture(dir, 1));
    int same = 1;
    for (int i = 0; i < WIDE * VIDEO_HEIGHT; i++)
        same &= seen[3 * i] == palette[3 * picture[i]] && seen[3 * i + 1] == palette[3 * picture[i] + 1] && seen[3 * i + 2] == 7;
    CHECK(same);

    palette[0] = 99;   /* a palette change shows it again, with the new colours */
    video_set_palette(0, 256, palette);   /* frame 2 */
    CHECK(last_capture(dir, 2));
    CHECK(picture[0] == 0 && seen[0] == 99);   /* the first pixel is colour 0, now 99 */
    static uint8_t shown[sizeof seen];
    memcpy(shown, seen, sizeof shown);   /* frame 2: the wide picture */

    video_set_wide_picture(picture, WIDE + 1);   /* another width: not shown */
    video_flip();   /* frame 3: the menu frame over its backdrop */
    CHECK(last_capture(dir, 3));
    CHECK(memcmp(shown, seen, sizeof shown) != 0);
    static uint8_t frame[sizeof seen];
    memcpy(frame, seen, sizeof frame);

    video_set_wide_picture(picture, WIDE);
    video_set_wide_picture(NULL, 0);   /* cleared */
    video_flip();   /* frame 4: the same menu frame */
    CHECK(last_capture(dir, 4));
    CHECK(memcmp(frame, seen, sizeof frame) == 0);

    video_shutdown();
    platform_shutdown();
    for (unsigned i = 0; i < 5; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/frame_%05u.ppm", dir, i);
        remove(path);
    }
    return TEST_REPORT();
}
