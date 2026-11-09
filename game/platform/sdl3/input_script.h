/* the input script and the key watch (input_script.c); platform_sdl3.c calls these, nothing else does */
#ifndef TNFS_INPUT_SCRIPT_H
#define TNFS_INPUT_SCRIPT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

void input_script_load(void);                          /* TNFS_INPUT_SCRIPT / TNFS_INPUT_SETTLE, once at platform_init */
bool input_script_active(void);                        /* a script was loaded */
void input_script_latch_presses(void);
void input_script_apply_keys(void);
void input_script_apply_text(void);
void input_script_apply_mouse(void);
bool input_script_key_down(int scancode);
void input_script_note_present(void);                  /* a frame was presented */
void input_script_log_window_event(Uint32 type);       /* --verbose */
void input_script_watch_key_event(const SDL_Event *event);
void input_script_watch_check(void);
void input_script_watch_reset(void);

#endif
