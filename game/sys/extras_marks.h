/* port: the marks a record carries for the cheats that were on when it was set (the table "with cheats" shows them as
 * letters). One bit each; port_extras.h shares this enum. */
#ifndef TNFS_EXTRAS_MARKS_H
#define TNFS_EXTRAS_MARKS_H

/* EXTRAS_MARK_WARRIOR: the times of the Warrior (car 11), which the original records nothing for */
/* EXTRAS_MARK_LUNAR (dep55): a race run with the moon's gravity in effect (track_gravity.h): a cheat, as LUNAR was the original's code */
enum { EXTRAS_MARK_NITRO = 1, EXTRAS_MARK_EXPLODE = 2, EXTRAS_MARK_TURBO = 4, EXTRAS_MARK_WARRIOR = 8, EXTRAS_MARK_LUNAR = 16 };

#endif
