/* port: a circuit driven the other way round (tri_reverse.h). extras.cfg's `reverse` asks for it, TNFS_REVERSE=0|1 overrides
 * the file for the checks; 0, the default, is the exe. A race on a reversed track keeps no record and has no rival times
 * (the records' and the splits' tables are for the track's own direction). */
#ifndef TNFS_REVERSE_TRACK_H
#define TNFS_REVERSE_TRACK_H

/* Whether the settings ask for the next race to run backwards. */
int reverse_track_wanted(void);

/* The race now running: 1 when its track was reversed. Read by the records. */
void reverse_track_set(int on);
int reverse_track_running(void);

#endif
