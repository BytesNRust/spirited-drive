#include "game/frontend/audio_menu.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { SONG, RACE, MENU, EFFECTS, SPEECH, MODE, THREE_D, DONE, CANCEL, ITEMS };
enum { SLIDERS = 3, SONGS = 16, MODES = 3, PAUSE = 2, ALT_SOUND = 5 };
enum { MOVED = 0x47, CHOSEN = 0x48, TEST = 0x4a, WAIT = 0x2ee0, BANNER_WAIT = 200, REST = 0x19, RESTART = 100,
       WIDE = 0x280, ALL = 0x100 };
enum { SLIDER_X = 0x12c, SCRATCH_W = 0xc6, SCRATCH_H = 0x1d, KNOB = 0xb2, TRACK = 0xb8, TOP_VOLUME = 0x7f,
       CLICK_LEFT = 0x131, CLICK_RIGHT = 0x1ec, CLICK_X = 0x132 };

/* 004014c0 / 004014d2: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 240, 240, 240, 240, 240, 240, 240, 40, 565 };
static const int16_t POINTER_Y[ITEMS] = { 114, 158, 201, 244, 287, 329, 372, 450, 450 };
/* 004014e4: config.dat's songs' tags */
static const char *const SONG_TAG[SONGS] = { "Nony", "Shuy", "Shuo", "Shug", "Ramo", "Toxo", "Scuo", "Oveo",
                                             "Rouo", "Powo", "Filg", "Odyg", "Aaeg", "Hidg", "Fung", "Chog" };
/* 00401524..0040155c: the speech's, the mode's and 3D's settings, lit and dark (the dark mode's twice over) */
static const char *const MODE_LIT[MODES] = { "16bl", "8bsl", "8bil" };
static const char *const MODE_DARK[MODES] = { "16bd", "8bsd", "8bid" };
static const char *const SPEECH_LIT[2] = { "Oful", "Onul" };
static const char *const SPEECH_DARK[2] = { "Ofud", "Onud" };
static const char *const THREE_D_LIT[2] = { "Ofdl", "Ondl" };
static const char *const THREE_D_DARK[2] = { "Ofdd", "Ondd" };
static const char *const LIT[ITEMS] = { "Sonl", "Racl", "Menl", "Soul", "Spel", "Audl", "3del", "Donl", "Canl" };
/* the sound test's dark label is the song's ("Sond"), as in the original */
static const char *const DARK[ITEMS] = { "Sond", "Racd", "Mend", "Sond", "Sped", "Audd", "3ded", "Dond", "Cand" };
/* the sliders: their pictures lit and dark, their rows, the rows a click takes, their volumes */
static const char *const SLIDER_LIT[SLIDERS] = { "Bol1", "Bol2", "Bol3" };
static const char *const SLIDER_DARK[SLIDERS] = { "Box1", "Box2", "Box3" };
static const int16_t SLIDER_Y[SLIDERS] = { 0x8d, 0xb8, 0xe3 };
static const int16_t CLICK_TOP[SLIDERS] = { 0x93, 0xbe, 0xe9 };
static const int16_t CLICK_BOTTOM[SLIDERS] = { 0xa4, 0xcf, 0xfa };
static const int VOLUME[SLIDERS] = { AUDIO_RACE, AUDIO_MENU, AUDIO_EFFECTS };
/* the bands 00463f80 is asked about, in turn */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0x143 };
static const int16_t BAND_RIGHT[ITEMS] = { WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, 0x141, WIDE };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x8a, 0xb4, 0xdf, 0x10a, 0x136, 0x15f, 0x18e, 0x18e };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x89, 0xb3, 0xde, 0x109, 0x135, 0x15e, 0x18d, 0x1e0, 0x1df };

typedef struct Menu {
    int32_t bank;              /* ebp */
    int32_t scratch;           /* esp + 0x490: a slider drawn before it is put on the screen */
    const uint8_t *pal;        /* esp + 0x494 */
    int16_t selected;          /* esp + 0x4dc */
    int16_t previous;          /* esp + 0x4b8 */
    int16_t pos[SLIDERS];      /* esp + 0x4b4, 0x4b0, 0x4a4: the knobs, 0..0xb8 */
    int16_t mode_kept;         /* esp + 0x4cc: the mode on entry, then as last chosen */
    int changed;               /* esp + 0x4d8 */
    int redraw;                /* esp + 0x4a8 */
    int fade;                  /* esp + 0x4bc: the palette faded in on the next draw */
    int armed;                 /* esp + 0x49c: the keys taken */
    int32_t rest;              /* esp + 0x4a0: the ticks summed while a key is held */
    int released;              /* esp + 0x4d4: the buttons seen up */
    int race;                  /* esp + 0x4c4: on the race music's items, 00464240 and 00464ef4 */
    int restart;               /* esp + 0x4c0: the song started again 100 clock ticks after it was chosen */
    int32_t chosen_at;         /* esp + 0x498 */
    int16_t leave;             /* esp + 0x4ac */
    uint8_t kept[AUDIO_KEPT];  /* esp + 0x300 */
    char text[64];             /* esp + 0x3ec: the German banner */
    uint8_t rgb[AUDIO_PALETTE];  /* esp + 0 */
} Menu;

static int16_t get(const AudioMenuState *st, int at)
{
    int16_t v;
    memcpy(&v, st->config + at, sizeof v);
    return v;
}

static void put(const AudioMenuState *st, int at, int16_t v)
{
    memcpy(st->config + at, &v, sizeof v);
}

static void move_to(int16_t item, const AudioMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Menu *m, const char *tag, const AudioMenuOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void blit_masked(const Menu *m, const char *tag, const AudioMenuOps *o)
{
    o->blit_masked(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void music(const Menu *m, const AudioMenuOps *o)
{
    if (m->race) o->race_music(o->ctx);
    else o->music(o->ctx);
}

/* the first band the pointer is in becomes the selection; the music's sliders' bands set its volume */
static void follow(Menu *m, int32_t mode, const AudioMenuState *st, const AudioMenuOps *o)
{
    const int mode_offered = (int16_t)mode != PAUSE && *st->card_bit1_clear == 0;
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], BAND_RIGHT[i], BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (i == MODE && (!mode_offered || *st->sound_alt == ALT_SOUND)) continue;
        if (i == THREE_D && !mode_offered) continue;
        if (i >= RACE && i <= EFFECTS) {
            m->race = i != MENU;
            o->music_volume(o->ctx, get(st, i == MENU ? AUDIO_MENU : AUDIO_RACE));
        }
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

static void draw_sliders(const Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    for (int k = 0; k < SLIDERS; k++) {
        o->select(o->ctx, m->scratch);
        o->blit_shape(o->ctx, o->shape(o->ctx, m->bank, m->selected == k + RACE ? SLIDER_LIT[k] : SLIDER_DARK[k]),
                      0, 0);
        o->clip(o->ctx, 6, 0xc8, 0, 0x64);
        o->blit_shape(o->ctx, o->shape(o->ctx, m->bank, "Bor1"), m->pos[k] - KNOB, 6);
        o->select(o->ctx, *st->screen);
        o->blit_at(o->ctx, o->picture_shape(o->ctx, m->scratch), SLIDER_X, SLIDER_Y[k]);
    }
}

/* a setting's item: its label and setting when lit, its setting alone when dark */
static void draw_setting(const Menu *m, int i, const char *const *lit, const char *const *dark, int16_t v,
                         const AudioMenuOps *o)
{
    if (m->selected == i) {
        blit(m, LIT[i], o);
        blit(m, lit[v], o);
    } else {
        blit(m, dark[v], o);
    }
}

static void draw(Menu *m, int32_t mode, const AudioMenuState *st, const AudioMenuOps *o)
{
    const int16_t audio_mode = get(st, AUDIO_MODE);
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    draw_sliders(m, st, o);
    blit(m, SONG_TAG[get(st, AUDIO_SONG)], o);
    for (int i = SONG; i <= EFFECTS; i++)
        if (m->selected == i) blit(m, LIT[i], o);
    draw_setting(m, SPEECH, SPEECH_LIT, SPEECH_DARK, get(st, AUDIO_SPEECH), o);
    draw_setting(m, MODE, MODE_LIT, MODE_DARK, audio_mode, o);
    draw_setting(m, THREE_D, THREE_D_LIT, THREE_D_DARK, get(st, AUDIO_3D), o);
    if (m->selected == DONE) blit(m, LIT[DONE], o);
    if (m->selected == CANCEL) blit(m, LIT[CANCEL], o);
    if ((int16_t)mode == PAUSE || *st->card_bit1_clear) {
        blit(m, "Audm", o);
        blit(m, MODE_DARK[audio_mode], o);
        blit(m, "3dem", o);
    }
    if (*st->sound_alt == ALT_SOUND) {
        blit(m, "Audm", o);
        blit(m, MODE_DARK[audio_mode], o);
    }
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->palette_set(o->ctx, m->rgb, 0, ALL, m->pal + 0x10, m->pal[0]);
        o->text_styles(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
    m->redraw = 0;
}

/* an item drawn lit or dark over the screen as it is; the sliders' labels masked too */
static void draw_item(Menu *m, uint16_t i, int lit, const AudioMenuState *st, const AudioMenuOps *o)
{
    if (i > CANCEL) return;
    blit(m, (lit ? LIT : DARK)[i], o);
    if (i >= RACE && i <= EFFECTS) blit_masked(m, (lit ? LIT : DARK)[i], o);
    if (i == SONG || (i == EFFECTS && !lit)) blit(m, SONG_TAG[get(st, AUDIO_SONG)], o);
    else if (i == SPEECH) blit(m, (lit ? SPEECH_LIT : SPEECH_DARK)[get(st, AUDIO_SPEECH)], o);
    else if (i == MODE) blit(m, (lit ? MODE_LIT : MODE_DARK)[get(st, AUDIO_MODE)], o);
    else if (i == THREE_D) blit(m, (lit ? THREE_D_LIT : THREE_D_DARK)[get(st, AUDIO_3D)], o);
    if (!lit && i == EFFECTS) o->stop_4a(o->ctx);
    if (!lit && i >= RACE && i <= EFFECTS) m->released = m->armed = 0;
}

/* on a slider the music is served for a tick and the whole screen drawn next; else the two items again */
static void redraw_items(Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    m->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    if (m->selected >= RACE && m->selected <= EFFECTS) {
        o->deadline_set(o->ctx, 1);
        while (!o->deadline_passed(o->ctx)) {
            music(m, o);
            if (*st->pointer_active) o->pointer_poll(o->ctx);
        }
    } else {
        o->pointer_hide(o->ctx);
        o->lock(o->ctx);
        draw_item(m, (uint16_t)m->previous, 0, st, o);
        draw_item(m, (uint16_t)m->selected, 1, st, o);
        m->previous = m->selected;
    }
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
}

/* a slider's volume from its knob (1 for the knob at 1), given to the sound */
static void set_volume(Menu *m, int k, const AudioMenuState *st, const AudioMenuOps *o)
{
    put(st, VOLUME[k], (int16_t)(m->pos[k] * TOP_VOLUME / TRACK));
    if (m->pos[k] == 1) put(st, VOLUME[k], 1);
    if (k == 2) o->effects_volume(o->ctx, get(st, AUDIO_EFFECTS));
    else o->music_fade_to(o->ctx, get(st, VOLUME[k]));
}

/* the volumes kept while the track music was silent put back */
static void unsilence(int k, const AudioMenuState *st, const AudioMenuOps *o)
{
    put(st, AUDIO_EFFECTS, st->kept[3]);
    put(st, AUDIO_MENU, st->kept[2]);
    put(st, AUDIO_RACE, st->kept[0]);
    o->voice_volume(o->ctx, *st->voice, get(st, AUDIO_EFFECTS));
    o->voice_volume(o->ctx, *st->voice_4a, get(st, AUDIO_EFFECTS));
    o->voice_volume(o->ctx, 4, o->effects_level(o->ctx));
    if (k == 2) o->music_fade_to(o->ctx, get(st, AUDIO_MENU));
    *st->track_silent = 0;
}

/* a click on a slider's track: the knob under the pointer, its volume when it changed */
static void click_slider(Menu *m, int k, const AudioMenuState *st, const AudioMenuOps *o)
{
    if (!o->pointer_in_rect(o->ctx, CLICK_LEFT, CLICK_RIGHT, CLICK_TOP[k], CLICK_BOTTOM[k])) return;
    int16_t p = (int16_t)(*st->pointer_x - CLICK_X);
    if (p < 0) p = 0;
    if (p > TRACK) p = TRACK;
    m->pos[k] = p;
    if (get(st, VOLUME[k]) == p * TOP_VOLUME / TRACK) return;
    if (*st->track_silent) unsilence(k, st, o);
    set_volume(m, k, st, o);
    m->changed = 1;
}

static void put_back(Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    memcpy(st->config, m->kept, sizeof m->kept);
    o->music_fade_to(o->ctx, get(st, AUDIO_MENU));
    m->leave = 1;
}

/* the mode: refused in a link game (the banner 200 ticks, then all drawn again); else the next offered, the sound
 * started again in it when on */
static void choose_mode(Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    if (*st->lost) {
        o->pointer_hide(o->ctx);
        if (*st->german) {
            snprintf(m->text, sizeof m->text, "Konnte Audio-Modus nicht in Netzwerk-Modus %cndern.\n", 0x84);
            o->banner(o->ctx, m->text);
        } else {
            o->banner(o->ctx, "Cannot change audio mode in Multi-player modes.");
        }
        o->deadline_set(o->ctx, BANNER_WAIT);
        while (!o->deadline_passed(o->ctx)) {
            music(m, o);
            if (*st->pointer_active) o->pointer_buttons(o->ctx);
        }
        o->deadline_set(o->ctx, WAIT);
        m->redraw = 1;
        return;
    }
    put(st, AUDIO_MODE, (int16_t)((get(st, AUDIO_MODE) + 1) % MODES));
    if (*st->card_bit0 == 0 && get(st, AUDIO_MODE) == 0) put(st, AUDIO_MODE, 1);
    m->mode_kept = get(st, AUDIO_MODE);
    if (*st->sound_on) {
        o->banner(o->ctx, *st->german ? "Wechsle audio-modus" : "Changing audio mode");
        o->music_stop(o->ctx);
        o->sound_free(o->ctx);
        o->music_end(o->ctx);
        o->sound_card(o->ctx);
        if (get(st, AUDIO_SONG)) {
            o->track_music(o->ctx);
            o->music_feed(o->ctx);
        }
        o->sound_load(o->ctx);
    }
    move_to(m->selected, o);
    m->redraw = 1;
}

/* speech or 3D toggled, its setting lit */
static void toggle(Menu *m, int at, const char *const *lit, const AudioMenuState *st, const AudioMenuOps *o)
{
    o->sound(o->ctx, CHOSEN);
    put(st, at, get(st, at) ^ 1);
    o->pointer_hide(o->ctx);
    blit(m, lit[get(st, at)], o);
    m->redraw = 1;
    m->released = m->armed = 0;
    move_to(m->selected, o);
}

/* Space, Enter or the left button: the item chosen when the keys are taken */
static void chosen(Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (m->armed) {
        o->select(o->ctx, *st->screen);
        switch ((uint16_t)m->selected) {
        case SONG:
            o->music_stop(o->ctx);
            o->music_end(o->ctx);
            put(st, AUDIO_SONG, (int16_t)((get(st, AUDIO_SONG) + 1) % SONGS));
            m->chosen_at = o->clock(o->ctx);
            m->restart = m->changed = 1;
            break;
        case RACE:
        case MENU:
        case EFFECTS:
            click_slider(m, m->selected - RACE, st, o);
            break;
        case SPEECH:
            toggle(m, AUDIO_SPEECH, SPEECH_LIT, st, o);
            break;
        case MODE:
            o->sound(o->ctx, CHOSEN);
            choose_mode(m, st, o);
            m->released = m->armed = 0;
            break;
        case THREE_D:
            toggle(m, AUDIO_3D, THREE_D_LIT, st, o);
            break;
        case DONE:
            o->sound(o->ctx, CHOSEN);
            m->leave = 1;
            break;
        case CANCEL:
            o->sound(o->ctx, CHOSEN);
            put_back(m, st, o);
            break;
        default:
            break;
        }
    }
    m->rest = 0;
    o->ticks(o->ctx);
}

/* Up: back one, Cancel to 3D, the song to Done, over the items not offered */
static void step_up(Menu *m, int32_t mode, const AudioMenuState *st)
{
    if (m->selected == CANCEL) m->selected = DONE;
    m->selected--;
    if (m->selected < 0) m->selected = DONE;
    if (((int16_t)mode == PAUSE || *st->card_bit1_clear) && m->selected == THREE_D) m->selected = SPEECH;
    if (*st->sound_alt == ALT_SOUND && m->selected == MODE) m->selected = SPEECH;
}

/* Down: on one, Done and Cancel to the song, over the items not offered */
static void step_down(Menu *m, int32_t mode, const AudioMenuState *st)
{
    const int16_t was = m->selected;
    m->selected = (int16_t)((m->selected + 1) % 8);
    if (was == CANCEL) m->selected = SONG;
    if (((int16_t)mode == PAUSE || *st->card_bit1_clear) && m->selected == MODE) m->selected = DONE;
    if (*st->sound_alt == ALT_SOUND && m->selected == MODE) m->selected = THREE_D;
}

/* Left / Right: a slider's knob one along, Done and Cancel to each other */
static void slide(Menu *m, int by, const AudioMenuState *st, const AudioMenuOps *o)
{
    const int16_t i = m->selected;
    if (i >= RACE && i <= EFFECTS) {
        const int k = i - RACE;
        o->pointer_off(o->ctx);
        if (by > 0 ? m->pos[k] < TRACK : m->pos[k] > 0) m->pos[k] = (int16_t)(m->pos[k] + by);
        set_volume(m, k, st, o);
        move_to(i, o);
        m->changed = 1;
    } else if (i == DONE || i == CANCEL) {
        o->pointer_off(o->ctx);
        m->previous = i;
        m->selected = i == DONE ? CANCEL : DONE;
        move_to(m->selected, o);
        m->changed = 1;
    }
    m->armed = 0;
}

/* a key other than the choosing ones: Escape puts back and leaves, the arrows step */
static void step(Menu *m, int32_t mode, uint16_t k, const AudioMenuState *st, const AudioMenuOps *o)
{
    if (k == ESCAPE) {
        if (m->selected == EFFECTS) o->stop_4a(o->ctx);
        o->sound(o->ctx, CHOSEN);
        put_back(m, st, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        if (k == UP) step_up(m, mode, st);
        else step_down(m, mode, st);
        move_to(m->selected, o);
        m->changed = 1;
        m->armed = 0;
    } else if (k == LEFT || k == RIGHT) {
        slide(m, k == RIGHT ? 1 : -1, st, o);
    }
    m->rest = 0;
    o->ticks(o->ctx);
}

static void open_screen(Menu *m, int32_t mode, const AudioMenuState *st, const AudioMenuOps *o)
{
    memcpy(m->kept, st->config, sizeof m->kept);
    m->mode_kept = get(st, AUDIO_MODE);
    if (get(st, AUDIO_SONG) > 3 || (int16_t)mode == PAUSE) {
        o->music_fade_out(o->ctx);
        o->music_end(o->ctx);
        o->track_music(o->ctx);
    }
    if (*st->card_bit0 == 0 && get(st, AUDIO_MODE) == 0) put(st, AUDIO_MODE, 2);
    for (int k = 0; k < SLIDERS; k++)
        m->pos[k] = *st->track_silent ? st->kept[k == 0 ? 0 : k + 1]
                                      : (int16_t)(get(st, VOLUME[k]) * TRACK / TOP_VOLUME);
    m->scratch = o->picture_new(o->ctx, SCRATCH_W, SCRATCH_H, 0);
    move_to(m->selected, o);
    m->bank = o->load_art(o->ctx, "option/audio.fsh");
    *st->inverse = o->load_inverse(o->ctx, "option/audio.inv");
    o->music_feed(o->ctx);
    m->pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, m->pal + 0x10, m->pal[0]);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
}

static void close_screen(Menu *m, int32_t mode, const AudioMenuState *st, const AudioMenuOps *o)
{
    o->fade_out(o->ctx, 0, ALL);
    o->free_inverse(o->ctx);
    o->picture_free(o->ctx, m->scratch);
    o->free(o->ctx, m->bank);
    if (get(st, AUDIO_MODE) != m->mode_kept) {
        o->music_fade_out(o->ctx);
        o->sound_free(o->ctx);
        o->music_end(o->ctx);
        o->sound_card(o->ctx);
        o->sound_load(o->ctx);
    } else if ((int16_t)mode == PAUSE) {
        o->music_fade_out(o->ctx);
        o->music_end(o->ctx);
    }
    if ((int16_t)mode == PAUSE) {
        *st->small_stream = 1;
        o->music_start(o->ctx, "pause.asf");
    } else if (get(st, AUDIO_MODE) != m->mode_kept) {
        o->music_pick(o->ctx);
    }
    o->music_fade_to(o->ctx, get(st, AUDIO_MENU));
    o->music_feed(o->ctx);
    o->deadline_set(o->ctx, WAIT);
    o->pointer_mark(o->ctx);
}

/* the song started again once chosen a while; the sound test played while on it, stopped on leaving it */
static void serve(Menu *m, const AudioMenuState *st, const AudioMenuOps *o)
{
    if (m->restart && (int32_t)((uint32_t)o->clock(o->ctx) - (uint32_t)m->chosen_at) > RESTART) {
        m->restart = 0;
        if (get(st, AUDIO_SONG) != 0) {
            o->track_music(o->ctx);
            o->music_feed(o->ctx);
        }
    }
    if (m->selected != EFFECTS && m->previous == EFFECTS) o->stop_4a(o->ctx);
    else if (m->selected == EFFECTS && (int16_t)o->done_4a(o->ctx)) o->sound(o->ctx, TEST);
}

/* 004015dc */
int32_t audio_menu(const AudioMenuState *st, int32_t mode, const AudioMenuOps *ops)
{
    static Menu zero;
    Menu m = zero;
    m.fade = m.redraw = m.armed = m.released = m.race = 1;
    open_screen(&m, mode, st, ops);
    do {
        serve(&m, st, ops);
        follow(&m, mode, st, ops);
        if (m.redraw) draw(&m, mode, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t k = (uint16_t)(m.race ? ops->race_key(ops->ctx) : ops->key(ops->ctx));
        if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m.released)) {
            chosen(&m, st, ops);
        } else if (k == 0) {
            m.armed = 1;
        } else if (m.armed) {
            step(&m, mode, k, st, ops);
        }
        music(&m, ops);
        if (m.fade) continue;
        if (*st->net_off == 0) ops->net_sync(ops->ctx);
        if (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released) || *st->quit == 1) {
            if (m.selected == EFFECTS) ops->stop_4a(ops->ctx);
            ops->sound(ops->ctx, CHOSEN);
            m.leave = 1;
        }
    } while (!m.leave);
    close_screen(&m, mode, st, ops);
    return 0;
}
