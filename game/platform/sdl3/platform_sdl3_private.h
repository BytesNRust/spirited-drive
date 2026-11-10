/* The SDL3 platform files' shared names (platform_sdl3.c, platform_audio_sdl3.c, platform_input_sdl3.c, platform_mods_sdl3.c,
 * platform_user_sdl3.c, platform_present_sdl3.c, platform_pool_sdl3.c), split from platform_sdl3.c with no change in what they do. */
#ifndef TNFS_PLATFORM_SDL3_PRIVATE_H
#define TNFS_PLATFORM_SDL3_PRIVATE_H

#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern SDL_Window *sdl3_window;
extern SDL_Renderer *sdl3_renderer;
extern SDL_Texture *sdl3_texture;
extern uint32_t *sdl3_rgb_buffer;
extern int sdl3_fb_width;
extern int sdl3_fb_height;
extern const char *sdl3_capture_dir;
extern int sdl3_capture_every;
extern unsigned sdl3_capture_from;
extern unsigned sdl3_presented_frames;
extern uint64_t sdl3_next_vblank_ns;
extern bool sdl3_window_focused;   /* the window has the keyboard focus: the system cursor is hidden over it then */
extern char sdl3_user_override[1024];   /* --user DIR */
extern SDL_AudioStream *sdl3_audio;     /* the open audio stream (platform_audio_sdl3.c), closed by platform_shutdown */

/* the name of the player's folder, in one place (it may change); LEGACY_* is where it was before, moved from once */
#define USER_FOLDER_NAME "TNFS Restomod"
#define LEGACY_USER_ORG "tnfs"
#define LEGACY_USER_APP "tnfs"

const char *sdl3_user_dir(char *out, size_t out_size);
const char *sdl3_user_path(const char *name, char *out, size_t out_size);
void sdl3_open_log(void);
void sdl3_cursor_apply(void);
#ifndef __EMSCRIPTEN__
void sdl3_pool_stop(void);
#endif

#endif
