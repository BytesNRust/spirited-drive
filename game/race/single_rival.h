/* port: the single race's rival from the menus. 0042702c puts the opponent panel's pick (00524e81) through 00426fec
 * into the second record, and 00457870 (race_models.c) then races every computer car as that model, or the eight
 * different cars of 004c5628 when the pick is the whole field (the panel's 8). */
#ifndef TNFS_SINGLE_RIVAL_H
#define TNFS_SINGLE_RIVAL_H

enum { SINGLE_RIVAL_FIELD = 8 };   /* entries[1].car asking 00457870 for the eight-car field */

/* The model entries[1].car takes in a single race: the menus' pick (race_head_to_head_model of the panel, 0..8) when
 * the race comes from them, else the whole field, which is what --drive without menus stands in with. */
int single_rival_car(int from_menus, int menu_pick);

#endif
