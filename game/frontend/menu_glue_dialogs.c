/* The checked menus' boxes: the banner, the yes/no and name boxes, the save and replay boxes (menu_glue.h; split from menu_glue.c by responsibility). */
#include "game/frontend/menu_glue_private.h"

static const MessageBannerOps BANNER = { width, picture_new, picture_shape, shade_table, select_picture,
                                         select_default, grab, shade_set, shade_polygon, colour, frame, styles,
                                         style, centred, blit_at, blit_picture, flip, picture_free, NULL };
static uint8_t banner_palette[PALETTE_PUT_BYTES];  /* 00520dd8 */

/* 0046667c (message_banner.c) */
static void banner(void *ctx, const char *text)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    message_banner_show(text, &in, &BANNER);
}

/* 00467214 (message_banner.c) */
void menu_glue_banner_wait(void *ctx, const char *text, int32_t time)
{
    (void)ctx;
    const MessageBannerWaitOps ops = { music, picture_new, picture_shape, select_picture, grab, banner,
                                       deadline_set, blit_at, picture_free, show, deadline_passed, key, buttons,
                                       select_default, blit_picture, flip, NULL };
    int active = pointer_active(), held = 0;
    message_banner_wait(text, time, ea_screen(), &active, &held, &ops);
}

static void dialog_poll(void *ctx, DialogPointer *p)
{
    (void)ctx;
    int x, y;
    pointer_sync_from_os();
    pointer_position(&x, &y);
    p->x = x, p->y = y;
}
/* 00441798 on the box's own short 00524790 (ctx its DialogPointer) */
static void dialog_show(void *ctx) { pointer_show(), ((DialogPointer *)ctx)->active = pointer_active(); }
static int32_t dialog_buttons(void *ctx, DialogPointer *p)
{
    dialog_poll(ctx, p);
    return pointer_read_buttons();
}
static void dialog_mark(void *ctx, DialogPointer *p)
{
    (void)ctx;
    p->mark_x = (int16_t)p->x, p->mark_y = (int16_t)p->y;
    pointer_mark();
}
static void ramp(void *ctx, int use_shadow) { (void)ctx; text_ramp(use_shadow); }
static void draw_text(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }

/* 00463934 (text_box.c) */
static void boxed(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    const TextBoxOps ops = { width, picture_new, picture_shape, select_picture, grab, ramp, draw_text, nothing,
                             blit_at, select_default, blit_picture, flip, picture_free, NULL };
    text_box_show(text, x, y, ea_screen(), &ops);
}

/* 00465f4c (yes_no_box.c) */
int32_t menu_glue_yes_no(void *ctx, const char *text)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const YesNoOps ops = { &BANNER, shadowed, boxed, nothing, dialog_poll, dialog_show, dialog_buttons, click, move_to,
                           dialog_mark, key, music, sound, drain, &p };
    const int32_t yes = yes_no_box(text, resource_german(), &p, &in, &ops);
    *pointer_active_word() = (int16_t)p.active;  /* 00465f4c sets and clears the one word itself */
    return yes;
}
static int32_t typed(void *ctx) { (void)ctx; return input_next_char(); }
static int32_t pointer_poll(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }

/* 0043d4c0 for the name box: the menus' joystick button (a pad's button too, as the menus read it) in the state's joystick_key,
 * which the box takes for Enter (the exe's own path, unwired in the port until now) */
static void pad_button(void *ctx) { ((TextEntryState *)ctx)->joystick_key = input_menu_button(); }

/* 004626d0 (text_entry.c); no German keyboard layout switch (004770a0 / 0047706c) */
static int32_t text_field(void *ctx, char *text, int32_t length, int32_t x, int32_t y, int32_t one)
{
    (void)ctx;
    TextEntryState st = { ea_screen(), (int16_t)pointer_active(), 0, 0 };
    const TextEntryOps ops = { width, picture_new, picture_shape, select_picture, select_default, nothing, grab, flip,
                               blit_at, blit_picture, blit_at, style, shadowed, key, music, drain, pointer_poll,
                               pad_button, typed, ticks, picture_free, nothing, nothing, &st };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = text_entry_edit(text, length, x, y, one, resource_german(), &st, &ops);
    pointer_bind_active(was);
    return result;
}

/* 004667fc (name_box.c) on the name at 00524df0, which is config.dat's: the port keeps it in frontend_player_name too,
 * so the image takes it at once (00466ae4 copies it to player 0's record, 0042702c to the race's entry) */
int32_t menu_glue_name_box(void *ctx, char *name)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const NameBoxOps ops = { &BANNER, nothing, text_field, sound, NULL };
    NameBoxState st = { &menu_cheats.keys_506, &menu_cheats.keys_50a };
    const int32_t taken = name_box_show(name, resource_german(), &in, &st, &ops);
    config_dat_set_name(config_current(), name);
    return taken;
}
/* 0042a680 / 0042a9f8's calls: ctx the game state; the saves' and replays' directory the player's ("") */
int16_t ask(void *ctx, const char *text) { return (int16_t)menu_glue_yes_no(ctx, text); }
static int32_t exists(void *ctx, const char *path) { (void)ctx; return save_files_exists(path); }
static void remove_file(void *ctx, const char *path) { (void)ctx; save_files_remove(path); }
static int32_t save_game(void *ctx, const char *path) { return game_file_save(path, ctx, &save_files_ops); }
static int32_t save_replay(void *ctx, const char *path) { (void)ctx; return game_file_copy_replay(path, &save_files_ops); }
static void redraw(void *ctx, int32_t eax, int32_t edx) { (void)ctx, (void)eax, (void)edx; pointer_show(); }

/* 0042a680 (save_box.c) */
void glue_save_box(char *name, uint8_t *config, uint8_t *game, int16_t lost_vegas)
{
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const SaveBoxOps ops = { &BANNER, text_field, sound, exists, ask, remove_file, save_game, menu_glue_banner_wait, redraw,
                             game };
    SaveBoxState st = { config, game, lost_vegas, "" };
    save_box_show(name, resource_german(), &in, &st, &ops);
}

/* 0042a9f8 (replay_box.c): the last race's Replay.rpl (0042a5e8 reads gamedata/replay/, the player's directory)
 * copied to the name */
void glue_replay_box(char *name, uint8_t *config)
{
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const SaveBoxOps ops = { &BANNER, text_field, sound, exists, ask, remove_file, save_replay, menu_glue_banner_wait, redraw,
                             NULL };
    replay_box_show(name, resource_german(), &in, config, "", &ops);
}

void glue_banner_wait(void *ctx, const char *text, int32_t time) { menu_glue_banner_wait(ctx, text, time); }

int32_t glue_yes_no(void *ctx, const char *text) { return menu_glue_yes_no(ctx, text); }
void glue_banner(void *ctx, const char *text) { banner(ctx, text); }
void glue_drain(void *ctx) { drain(ctx); }
void glue_boxed(void *ctx, const char *text, int32_t x, int32_t y) { boxed(ctx, text, x, y); }
