/* gamedata/config/config.dat: loaded by 0042a010, saved by 0042a504, defaults
 * built by 00429278.
 *
 * The file is a raw memory image of 0x5f52 (24402) bytes: the original copies
 * it to and from the global block 00524df0..0052ad41 in one go (the load only
 * accepts exactly that size). Fields are packed with no alignment, so most
 * 16-bit words sit at odd addresses. Offsets below are file offsets, the
 * address is the original global (offset + 00524df0). All integers are
 * little-endian.
 *
 *   offset  address   size  meaning
 *   0x0000  00524df0  42    player name, NUL-terminated ("Player", "Spieler"
 *                           in German); the menus accept up to 8 characters
 *   0x006b  00524e5b  u16   unknown (00432ff4 reads and rewrites it as the
 *                           low half of a 32-bit value, never as a number)
 *   0x006d  00524e5d  u16   race type (RACE_*)
 *   0x008b  00524e7b  u16   unknown (default 0)
 *   0x008f  00524e7f  u16   car flag B, derived from the player car (00465ec8)
 *   0x0091  00524e81  u16   opponent
 *   0x0095  00524e85  u16   track
 *   0x0099  00524e89  u16   car flag A, derived from the player car (00465ec8)
 *   0x009b  00524e8b  u16   player car
 *   0x00b1  00524ea1  u16   laps
 *   0x00b3  00524ea3  u16   difficulty
 *   0x006f..0x00af          other words set by 00429278 whose meaning is not
 *                           known: 00524e5f, 61, 63, 65, 67, 69, 6b, 6d, 6f,
 *                           71, 73, 75, 77, 79, 7d, 83, 87, 9f
 *   0x00b5..0x5f33          unknown records: nine-row and ten-row tables of a
 *                           name plus five numbers (the defaults come from
 *                           string tables at 004b9638..004baa44, such as
 *                           "Greenbeard", "Duke", "Silver Spyder"), looks like
 *                           the best-times / hall-of-fame data; kept verbatim
 *   0x5f34  0052ad24  u16   unknown (default 0)
 *   0x5f36  0052ad26  13 x u16  unknown; 00466ae4 copies these 26 bytes to
 *                           0052b298. Defaults: 0, 0, 0, 0x4b00, 0x4d00,
 *                           0x4800, 0x5000, 0x41, 0x5a (0x59 German), 0x20,
 *                           0x43 (0x4b German), 0x48, 0x4d. Probably the
 *                           keyboard bindings (the values are scancodes)
 *   0x5f50  0052ad40  u16   unlock level 0..2, from tournament wins (written
 *                           by 00427ae0): 0 nothing, 1 "m" track variants,
 *                           the Lost Vegas track and the Warrior car, 2 also
 *                           the "r" variants (read in 00432ff4)
 *
 * The image is kept whole so the regions that are not understood survive a
 * load and save unchanged. */
#ifndef TNFS_CONFIG_DAT_H
#define TNFS_CONFIG_DAT_H

#include "game/frontend/race_settings.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CONFIG_DAT_SIZE = 0x5f52,
    CONFIG_DAT_CONTROLS = 0x5f36,  /* 0052ad26: the 13 control words (00413cb0's) */
    CONFIG_NAME_SIZE = 42
};

typedef struct {
    uint8_t image[CONFIG_DAT_SIZE];
} ConfigDat;

typedef enum {
    CONFIG_OK = 0,
    CONFIG_BAD_SIZE,
    CONFIG_NULL
} ConfigResult;

/* 00429278 with 004670e8's ticks (100 frames of a picture drawn): the race words, the graphics words by the
 * ticks' bracket, the records' defaults (config_records.c), the controls (two German), the modem's names. The
 * language only changes the name and two controls. */
void config_dat_defaults_measured(ConfigDat *c, int german, int16_t ticks);
/* port: 004670e8 not run; a machine of today draws the 100 frames in well under 300 ticks, the first bracket */
void config_dat_defaults(ConfigDat *c, int german);

/* 0042a010: accepts exactly CONFIG_DAT_SIZE bytes. */
ConfigResult config_dat_parse(ConfigDat *c, const uint8_t *data, size_t size);
/* 0042a504: writes CONFIG_DAT_SIZE bytes. Returns the size, 0 if cap is too small. */
size_t config_dat_serialise(const ConfigDat *c, uint8_t *out, size_t cap);

/* Copies at most CONFIG_NAME_SIZE - 1 characters and always terminates. */
void config_dat_name(const ConfigDat *c, char out[CONFIG_NAME_SIZE]);
void config_dat_set_name(ConfigDat *c, const char *name);

/* Race setup. get returns 0 (and leaves *s alone) when a stored value is out
 * of range, so a damaged file cannot reach the selection code. */
int config_dat_get_race(const ConfigDat *c, RaceSettings *s);
void config_dat_set_race(ConfigDat *c, const RaceSettings *s);
/* 00466ae4's 00524e83 = 0: a new session starts a stage on its first segment */
void config_dat_new_session(ConfigDat *c);

int config_dat_unlock_level(const ConfigDat *c);
void config_dat_set_unlock_level(ConfigDat *c, int level);
/* 004322f8's 00524e67 = 100: the menus' volume before config.dat is read (the intro's movies at 00464698's level) */
void config_dat_startup_volume(ConfigDat *c);
/* Unlocks as derived in 00432ff4 from the unlock level. */
Unlocks config_dat_unlocks(const ConfigDat *c);

/* 00464670: the short 00524e69 (default 0x7f) times 3 / 2, at most 0x7f.
 * Called on the race side (004027c0, 00402911, 00402a62) next to
 * 00473b4c; what the level is for is not known yet. */
int config_dat_level_e69(const ConfigDat *c);
/* 00464698: the short 00524e67 (default 100) times 12 / 10, at most 0x7f. */
int config_dat_level_e67(const ConfigDat *c);

/* 00465d54: 1 when two of the controls (the shorts 0052ad26..0052ad3c) clash.
 * The first three clash among themselves when equal and not 0; each of them
 * that is not 0 stands for a pair of the next six (it replaces them with
 * the marks 0 / 1, 2 / 3, 4 / 5), and then any two of the nine left that are
 * equal clash. */
int config_dat_controls_clash(const ConfigDat *c);

#endif
