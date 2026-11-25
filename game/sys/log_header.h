/* port: what a bug report needs at the head of the log, once the run knows it: the mode in use and where the game's data is (the player's name is not in the path: log_path.h). The version, the
 * system and the time are the platform's lines, written when the log opens. */
#ifndef TNFS_LOG_HEADER_H
#define TNFS_LOG_HEADER_H

/* "profile: Restored '96" and "data: <kind>: <folder>"; `kind` says where the folder came from ("your copy of the CD", ...) */
void log_header_write(const char *data_root, const char *kind);

#endif
