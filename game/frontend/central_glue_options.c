/* The Control Central glue's Options, Controls and Audio screens (central_glue.c's fourth part). */
#include "game/frontend/central_glue_private.h"

static const uint16_t OPT_AT[OPT_SHORTS] = { 0x97, 0x8d, 0x99, 0x8f, 0x6d, 0x95, 0x91, 0x9b, 0xad, 0xb1, 0xb3, 0x77 };
int16_t cg_opt[OPT_SHORTS];

void cg_opt_out(void)
{
    const ConfigDat *const config = config_current();
    for (int i = 0; i < OPT_SHORTS; i++) memcpy(&cg_opt[i], config->image + OPT_AT[i], sizeof cg_opt[i]);
}

void cg_opt_in(void)
{
    ConfigDat *const config = config_current();
    for (int i = 0; i < OPT_SHORTS; i++) memcpy(config->image + OPT_AT[i], &cg_opt[i], sizeof cg_opt[i]);
}

/* 00465ec8 on the screen's shorts */
void cg_opt_car_flags(void *ctx)
{
    cg_opt_in();
    cg_image_car_flags(ctx);
    cg_opt_out();
}

static int16_t controls_kept[CONTROLS_SHORTS];
static void controls_out(void) { memcpy(controls_kept, config_current()->image + CONTROLS_AT, sizeof controls_kept); }
static void controls_in(void) { memcpy(config_current()->image + CONTROLS_AT, controls_kept, sizeof controls_kept); }
static int32_t clash(void *ctx)
{
    (void)ctx;
    controls_in();
    return config_dat_controls_clash(config_current());
}
/* 00476f94: the arrows from the key queue, anything else typed from the text queue (Enter, Escape among them) */
int32_t cg_key_read(void *ctx)
{
    (void)ctx;
    static const int32_t ARROWS[] = { [PLATFORM_KEY_LEFT] = 0x4b00, [PLATFORM_KEY_RIGHT] = 0x4d00,
                                      [PLATFORM_KEY_UP] = 0x4800, [PLATFORM_KEY_DOWN] = 0x5000 };
    for (PlatformKey k; (k = platform_next_key()) != PLATFORM_KEY_NONE;)
        if (k <= PLATFORM_KEY_DOWN) return ARROWS[k];
    return input_next_char();
}
static int32_t capture(void *ctx, char *name)
{
    KeyCaptureState st = { pointer_active(), 0, 0, 0, 0 };
    const KeyCaptureOps ops = { keys_pending, music, capture_buttons, nothing, ticks, glue_drain, cg_key_read, nothing,
                                ctx };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = key_capture(name, resource_german(), &st, &ops);
    pointer_bind_active(was);
    return result;
}
/* port: the added pages' text, panels and saving (graphics_plus' routines below), and the pad as it is now */
static int32_t plus_text_width(void *ctx, const char *text);
static void controls_plus_save(void *ctx, const PortControls *cg_controls)
{
    (void)ctx;
    PortControls saved = *cg_controls;   /* its `custom` as the pages left it: Defaults makes a wheel pass straight through */
    if (!port_controls_save(&saved)) port_log("controls: controls.cfg could not be saved\n");
    platform_controls_set(&saved);
}
/* Test aid, not for play: TNFS_FAKE_PAD=<source> (button4, axis5+, axis0, hat0.8) is a gamepad that presses that
 * source a moment after each time it is asked afresh (a gap of 10 ticks), so that a script can capture a binding
 * without a pad. */
static int fake_pad(RawPad *out)
{
    enum { FRESH_GAP = 10, IDLE_CALLS = 20 };
    static ControlSource source;
    static int parsed, calls;
    static uint32_t last;
    const char *spec = getenv("TNFS_FAKE_PAD");
    if (spec == NULL) return 0;
    if (!parsed) {
        char line[64];
        snprintf(line, sizeof line, "gas=%s\n", spec);
        source = port_controls_parse(line, strlen(line)).bind[ACT_GAS][0];
        parsed = 1;
    }
    const uint32_t now = timer_ticks();
    if (now - last > FRESH_GAP) calls = 0;
    last = now;
    memset(out, 0, sizeof *out);
    out->gamepad = 1, out->buttons = 15, out->axes = 6, out->hats = 1;
    if (++calls <= IDLE_CALLS) return 1;
    switch (source.kind) {
    case SRC_BUTTON: if (source.index < out->buttons) out->button[source.index] = 1; break;
    case SRC_AXIS_NEG: out->axis[source.index] = -32767; break;
    case SRC_AXIS_POS:
    case SRC_AXIS_FULL: out->axis[source.index] = 32767; break;
    case SRC_HAT: out->hat[source.index] = (uint8_t)source.rest; break;
    default: break;
    }
    return 1;
}
static int32_t pad_raw(void *ctx, RawPad *out) { (void)ctx; return fake_pad(out) || platform_joy_raw(0, out); }
/* port: the configuration screens' clean background for the controls' pages 2 and 3, the same one the Graphics and
 * CUSTOMIZE screens use (glue_own_bgnd's copy, shared) */
static int32_t controls_bgnd(void *ctx, uint8_t pixels[CONTROLS_BGND_W * CONTROLS_BGND_H], uint8_t palette[768])
{
    (void)ctx;
    return glue_own_bgnd(pixels, palette);
}
static int32_t plus_key_name(void *ctx, int32_t scancode, char *out, size_t size)
{
    (void)ctx;
    return platform_key_name(scancode, out, size);
}
/* port: the controls screen, from Options (mode 0) and from the options after a race (mode 1), is the port's three pages
 * (controls.cfg's keys, the pad's bindings and its tuning), the exe's page left out */
void cg_controls(void *ctx, int32_t mode)
{
    static const int16_t no_joystick_b = 0, quit = 0;  /* no second joystick */
    static const int32_t no_calibration = 0;  /* 0043bc00 dropped: no Calibrate item */
    const int16_t german = (int16_t)resource_german();
    int32_t *const screen = ea_screen_slot();
    PortControls port = port_controls_load();
    ControlsPlus plus = { &port, 1, port_settings_profile() != PROFILE_RESTOMOD };   /* port: the three pages, the exe's left out (input_read.c reads the keys and the pad together) */
    controls_out();
    const ControlsMenuState st = { controls_kept, &german, &drive_joy_state()->present, &no_joystick_b, &no_calibration,
                                   pointer_active_word(), &cg_g.buttons, screen, &cg_g.inverse, cg_word_52b302(), &quit,
                                   &plus };
    const ControlsMenuOps ops = {
        picture_new, cg_flush, palette_clear, move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, fade_in,
        styles, poll, mark, in_rect, style, select_picture, blit, picture_shape, grab, key_name, shadowed, set_palette,
        select_default, flip, show, deadline_set, sound, nothing, blit_at, ticks, buttons, activate, cg_key, click, clash,
        glue_boxed, capture, cg_deadline_passed, music, fade_out, free_handle_inverse, no_mode, off, net_sync,
        picture_free, free_bank, ctx, cg_plus_text, plus_text_width, cg_plus_panel, controls_plus_save, pad_raw,
        central_page_arrow_size, central_page_arrow,
        /* port: the pages 2 and 3 over the clean background, with the title and the group headers as CUSTOMIZE's */
        controls_bgnd, glue_own_bgnd_shape, own_plus_text, own_title_text, own_header_text,
        /* port: the keyboard page's capture and key names, and the pages' subtitle */
        plus_key_down, plus_key_name, cg_own_subtitle_text, cg_own_note_text };
    controls_menu(&st, mode, &ops);
    controls_in();
}

/* 004015dc's pointer x (004c6110), read as it drags a slider: kept by its button and move polls */
static int16_t audio_pointer_x;
static void audio_track(void)
{
    int x, y;
    pointer_position(&x, &y);
    audio_pointer_x = (int16_t)x;
}
static int32_t audio_buttons(void *ctx) { (void)ctx; audio_track(); return pointer_read_buttons(); }
static void audio_activate(void *ctx) { (void)ctx; audio_track(); pointer_activate_if_moved(); }
static int32_t audio_effects(void *ctx) { (void)ctx; return cg_image_short(0x79); }

/* 004015dc from Options (mode 0) on config.dat's image: the card 0049212c(-1) answers is the port's mixer, 16-bit
 * stereo with no modes to choose (bit 0 set, bit 1 clear), so neither the mode nor 3D sound is offered and the mode
 * shown is 0049212c's 3, config.dat's 0 (one saved as 1 or 2 by an older build put back); the speech is not heard
 * yet */
void cg_audio(void *ctx, int32_t mode)
{
    enum { AUDIO_MODE_AT = 0x71 };  /* 00524e61 */
    static int16_t small_stream;
    static const int16_t no_modes = 1, bit0 = 1, sound_alt = 0, sound_on = 1;
    static const int32_t voice_4a = 0, voice = 0;
    const int16_t german = (int16_t)resource_german();
    memset(config_current()->image + AUDIO_MODE_AT, 0, sizeof(int16_t));
    audio_track();
    cg_opt_in();
    const AudioMenuState st = {
        config_current()->image + 9, menu_audio_silent(), menu_audio_kept(), &no_modes, &bit0, &sound_alt, &menu_words.link_game,
        &german, &sound_on, &voice_4a, &voice, &audio_pointer_x, pointer_active_word(), &cg_g.buttons, ea_screen_slot(),
        &cg_g.inverse, &small_stream, cg_word_52b302(), stage_word() };
    const AudioMenuOps ops = {
        music_fade_out, music_end, audio_track_music, picture_new, move_to, load_art, no_inverse, music_feed, shape,
        cg_palette_set, poll, mark, audio_clock, audio_stop_4a, audio_4a_done, sound, in_rect, audio_volume,
        select_picture, blit, blit_at, audio_blit_at, audio_clip, picture_shape, select_default, hide, nothing, flip,
        show, audio_buttons, styles, fade_in, deadline_set, cg_deadline_passed, nothing, music, nothing, blit_masked,
        ticks, audio_activate, cg_key, cg_key, click, music_cut, audio_voice, audio_effects, music_fade_to,
        audio_effects_volume,
        glue_banner, sound_free, nothing, sound_load, off, net_sync, fade_out, free_handle_inverse, picture_free,
        free_bank, music_pick, music_start, ctx };
    audio_menu(&st, mode, &ops);
    cg_opt_out();
}

