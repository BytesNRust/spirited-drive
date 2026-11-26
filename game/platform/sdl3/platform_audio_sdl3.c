/* The SDL3 platform's audio stream (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/key_latch.h"
#include "game/sys/data_path.h"
#include "game/sys/file_url.h"
#include "game/sys/port_log.h"
#include "tnfs_version.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/input_shared.h"
#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <unistd.h>   /* fsync */
#include <limits.h>   /* realpath: symbolic links out of the mods directory are refused */
#define TNFS_HAVE_REALPATH 1
#endif
#include "platform/sdl3/platform_sdl3_private.h"

SDL_AudioStream *sdl3_audio;
static PlatformAudioFill audio_fill;
static void *audio_user;

enum { AUDIO_FRAME = 4, AUDIO_CHUNK = 0x200 };  /* bytes per 16-bit stereo frame; frames per fill */

/* the device's thread, the stream locked: what it asks for, filled a chunk at a time */
static void SDLCALL audio_get(void *user, SDL_AudioStream *stream, int additional, int total)
{
    (void)user, (void)total;
    static int16_t chunk[2 * AUDIO_CHUNK];
    for (int left = additional / AUDIO_FRAME; left > 0; left -= AUDIO_CHUNK) {
        const uint32_t n = left < AUDIO_CHUNK ? (uint32_t)left : AUDIO_CHUNK;
        audio_fill(audio_user, chunk, n);
        if (!SDL_PutAudioStreamData(stream, chunk, (int)(n * AUDIO_FRAME))) {
            port_log("platform: audio: %s\n", SDL_GetError());
            return;
        }
    }
}

static SDL_AtomicInt device_added;   /* set by the event watch for a playback device that appeared, taken by platform_audio_device_added */
static bool audio_up;               /* SDL_INIT_AUDIO is ours: kept after a failed open, to hear a device appear */
static bool watching;

static bool SDLCALL device_event(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if (event->type == SDL_EVENT_AUDIO_DEVICE_ADDED && !event->adevice.recording) SDL_SetAtomicInt(&device_added, 1);
    return true;
}

bool platform_audio_open(int rate, PlatformAudioFill fill, void *user)
{
    if (sdl3_audio != NULL) return true;
    if (fill == NULL) return false;
    if (!audio_up) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            port_log("platform: no audio: %s\n", SDL_GetError());
            return false;
        }
        audio_up = true;
    }
    if (!watching) watching = SDL_AddEventWatch(device_event, NULL);
    audio_fill = fill, audio_user = user;
    SDL_SetAtomicInt(&device_added, 0);   /* before the try: a device that appears during a failing open is kept for the retry */
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, rate };
    sdl3_audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_get, NULL);
    if (sdl3_audio != NULL && !SDL_ResumeAudioStreamDevice(sdl3_audio)) {
        SDL_DestroyAudioStream(sdl3_audio);
        sdl3_audio = NULL;
    }
    if (sdl3_audio == NULL) {   /* the subsystem stays up: a device that appears later is heard (platform_audio_device_added) */
        port_log("platform: no audio device: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void platform_audio_close(void)
{
    if (sdl3_audio != NULL) SDL_DestroyAudioStream(sdl3_audio);
    sdl3_audio = NULL;
    if (watching) SDL_RemoveEventWatch(device_event, NULL);
    watching = false;
    if (audio_up) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audio_up = false;
}

bool platform_audio_device_added(void)
{
    return SDL_CompareAndSwapAtomicInt(&device_added, 1, 0);
}

void platform_audio_lock(void)
{
    if (sdl3_audio != NULL) SDL_LockAudioStream(sdl3_audio);
}

void platform_audio_unlock(void)
{
    if (sdl3_audio != NULL) SDL_UnlockAudioStream(sdl3_audio);
}
