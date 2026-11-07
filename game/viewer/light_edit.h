/* port (light_edit): the live editor of a track's light. With light_edit=1 in port.cfg (or TNFS_LIGHT_EDIT=1 for a launcher) a race with a made light shows, in the top-left corner,
 * the node the car is at and the values the light has there, and what the track light file did (read, or the light made in code and which of the port's settings differ from the ones
 * the file was made with: its retouches count either way). F10 holds the car where it is (the race stands still, the picture goes on: the light is looked at, not driven past);
 * F11 copies the node's retouch line ("brightness 120 120 100") to the clipboard to paste into data/track_light/<TRACK><A|B|C|N>.txt. The file is read again a second after it is
 * saved, without leaving the race, and so is the track's places file (the light and the dry zones are made again; `snow` and `snow_map` change textures at load and want the race restarted). Presentation only: drawn into a copy of the frame as the stats overlay is (race_stats.h). */
#ifndef TNFS_LIGHT_EDIT_H
#define TNFS_LIGHT_EDIT_H

#include <stddef.h>
#include <stdint.h>

enum { LIGHT_EDIT_KEY_FREEZE = 0x44, LIGHT_EDIT_KEY_COPY = 0x57, LIGHT_EDIT_CHECK_MS = 1000, LIGHT_EDIT_LINES = 11, LIGHT_EDIT_MESSAGE_MS = 3000 };   /* F10, F11 (set-1 codes) */

typedef struct {
    int on;
    int frozen;
    int freeze_down, copy_down;     /* the keys at the last frame: the actions are on the press */
    uint32_t checked_ms, hash;      /* the last look at the file, and its checksum */
    int hash_known;
    char message[96];
    uint32_t message_ms;
    uint8_t palette[768];
    int palette_known, text_ready;
    uint8_t dark;
} LightEdit;

/* 1 when the editor is asked for (TNFS_LIGHT_EDIT=1, or light_edit=1 in port.cfg) */
int light_edit_wanted(void);
void light_edit_start(LightEdit *e, int on);
/* the keys of a frame (the set-1 key table of the race's input) with the node the followed car is at, at `now_ms` */
void light_edit_keys(LightEdit *e, const uint8_t *keys, uint32_t node, uint32_t now_ms);
/* once a frame: 1 when the light file or the places file changed since the last look and the light was made again */
int light_edit_poll(LightEdit *e, uint32_t now_ms);
/* the frame with the editor's lines drawn on a copy of it when on; `frame` itself when off. Valid until the next call. */
const uint8_t *light_edit_compose(LightEdit *e, const uint8_t *frame, int width, int height, const uint8_t palette[768], uint32_t node);

#endif
