/* The SDL3 platform files' shared names (platform_sdl3.c, platform_audio_sdl3.c, platform_input_sdl3.c, platform_mods_sdl3.c,
 * platform_user_sdl3.c, platform_present_sdl3.c, platform_pool_sdl3.c), split from platform_sdl3.c with no change in what they do. */
#ifndef TNFS_PLATFORM_SDL3_PRIVATE_H
#define TNFS_PLATFORM_SDL3_PRIVATE_H

#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture *texture;
extern uint32_t *rgb_buffer;
extern int fb_width;
extern int fb_height;
extern const char *capture_dir;
extern int capture_every;
extern unsigned capture_from;
extern unsigned presented_frames;
extern uint64_t next_vblank_ns;
extern bool window_focused;   /* the window has the keyboard focus: the system cursor is hidden over it then */
extern char user_override[1024];   /* --user DIR */
extern SDL_AudioStream *audio;     /* the open audio stream (platform_audio_sdl3.c), closed by platform_shutdown */

/* the name of the player's folder, in one place (it may change); LEGACY_* is where it was before, moved from once */
#define USER_FOLDER_NAME "TNFS Restomod"
#define LEGACY_USER_ORG "tnfs"
#define LEGACY_USER_APP "tnfs"

const char *user_dir(char *out, size_t out_size);
const char *user_path(const char *name, char *out, size_t out_size);
void open_log(void);
void cursor_apply(void);
#ifndef __EMSCRIPTEN__
void pool_stop(void);
#endif

#endif
