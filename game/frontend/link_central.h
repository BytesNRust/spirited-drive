/* The link game in Control Central and the menus around the race (glue, no
 * oracle): 004104d0, 00411050, 004465e4, 00410c18, 0044633c, 00446284,
 * 004109b4 and 00432e90 on the link game's words (link_game.h) and the
 * menu's race settings, which the menus' glue binds while they run. */
#ifndef TNFS_LINK_CENTRAL_H
#define TNFS_LINK_CENTRAL_H

#include <stdint.h>

/* the menu's settings 004104d0 and 004465e4 read and write (config.dat's image in the original) */
typedef struct LinkMenu {
    int16_t *race_type;    /* 00524e5d */
    int16_t *track;        /* 00524e85 */
    int16_t *laps;         /* 00524ea1 */
    int16_t *difficulty;   /* 00524ea3 */
    int16_t *net_car;      /* 00524e9d */
    int16_t *nomercy;      /* 004c5cc2 */
    int16_t *tournament;   /* 0052b310 */
    int16_t *player_car;   /* 00524e8b: this machine's */
    int16_t *other_car;    /* 00524e8d: 8, by slot */
    int32_t *variant_m;    /* 004fbad8 */
    int32_t *variant_r;    /* 004fbadc */
} LinkMenu;

/* The menu's settings while it runs (NULL: none; the calls below then do nothing) */
void link_central_bind(const LinkMenu *menu);

void link_sync(void);                 /* 004104d0 */
int32_t link_wait(int16_t stage);     /* 00410c18: nonzero when all came */
int32_t link_ready(void);             /* 0044633c */
void link_host_ready(void);           /* 00446284 */
int16_t link_any_ready(void);         /* 004109b4 */
void link_kick(int32_t player);       /* 00432e90 */

/* the words Control Central reads of the link game */
int16_t link_view_only(void);         /* 0052b316 */
int16_t link_humans(void);            /* 004f81d2 */
const char *link_names(void);         /* 004f8166: 9 bytes a player */

#endif
