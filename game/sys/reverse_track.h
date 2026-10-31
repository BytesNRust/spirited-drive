/* port: a circuit driven the other way round (tri_reverse.h). extras.cfg's `reverse` offers it on Track Select, TNFS_REVERSE=0|1 overrides
 * the settings for the checks; 0, the default, is the exe. A race on a reversed track keeps no record, has no rival times
 * (the records' and the splits' tables are for the track's own direction) and leaves no Replay.rpl (the file has no flag, and
 * would play on the track's own way round). */
#ifndef TNFS_REVERSE_TRACK_H
#define TNFS_REVERSE_TRACK_H

/* Whether the next race runs backwards: extras.cfg's `reverse` offers the Track Select row, and the row's pick (this session's,
 * not saved) asks for it; TNFS_REVERSE=1 asks for it without either. */
int reverse_track_wanted(void);

/* The race now running: 1 when its track was reversed. Read by the records. */
void reverse_track_set(int on);
int reverse_track_running(void);

/* The Track Select row's value. */
void reverse_track_pick(int on);
int reverse_track_picked(void);

#endif
