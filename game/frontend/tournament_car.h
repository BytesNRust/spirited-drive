/* port (dep59 step 5, T7a2): a tournament's car for a track, from the original's table of the tracks (ten words with the hidden one): a number
 * outside it (an added track's 16..79, or -1) reads the first track's, never past the table */
#ifndef TNFS_TOURNAMENT_CAR_H
#define TNFS_TOURNAMENT_CAR_H

#define TOURNAMENT_CAR_OF(table, track) ((table)[(unsigned)(track) < sizeof(table) / sizeof((table)[0]) ? (unsigned)(track) : 0u])

#endif
