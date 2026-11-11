/* port: free laps. `laps=N` (port.cfg, TNFS_LAPS) sets a race's lap count to any 1..99 instead of the
 * exe's 2/4/6/8/12/16 of the difficulty; 0, the default, is the exe's. A race whose laps are not the exe's is a free race:
 * the code the lap count would break guards itself (RaceState.port_free_laps) and no record is written for it. */
#ifndef TNFS_FREE_LAPS_H
#define TNFS_FREE_LAPS_H

enum {
    FREE_LAPS_MAX = 99,      /* config.dat takes 1..99, the HUD shows two digits, lap_marks holds 108 */
    FREE_LAPS_SLOTS = 20     /* the lap times the game state keeps a player (the exe's 20 slots) */
};

/* The free lap count the settings ask for, 0..FREE_LAPS_MAX: TNFS_LAPS when set to a number, else port.cfg's laps. */
int port_laps(void);

/* Whether `laps` is a count the exe offers (2, 4, 6, 8, 12 or 16). */
int free_laps_is_exe(int laps);

/* The laps choice (0..2) of a track's tables of best totals that a race of `laps` laps belongs to: the track's own
 * counts are 2, 6, 12 on tracks 4..7 and 4, 8, 16 on the circuits 3 and 8. -1 for any other count, and for a stage. */
int free_laps_choice(int track, int laps);
/* The lap count of laps choice `choice` (0..2) of a track's tables: 2, 6, 12 on tracks 4..7, 4, 8, 16 on the circuits 3 and 8; 0 for a stage or out of range. */
int free_laps_exe_laps(int track, int choice);

/* The race now running: its free lap count, or 0 when its laps are the exe's. Read by the screens after the race. */
void free_laps_set(int laps);
int free_laps_count(void);
/* `exe_laps` as a screen would show it, or the free count when a free race is the last one. */
int free_laps_or(int exe_laps);

#endif
