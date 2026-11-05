/* port (one time): port.cfg used to hold every setting and extras.cfg the switches. A port.cfg that still carries settings moves them to the mode's file, keeps a copy
 * as port.cfg.bak and is left with profile= and psx_dir=. A named mode keeps its name and what it left alone that stays free in it; custom, or no profile line, becomes
 * Restomod, the only mode where all of it stays free, and every key it set stays set even at the table's value. Goes away once no old files are left. */
#ifndef TNFS_PORT_OLD_FILES_H
#define TNFS_PORT_OLD_FILES_H

#include <stdbool.h>
#include <stddef.h>

/* 1 when the text of a port.cfg carries a key other than profile and psx_dir */
int port_old_files_carried(const char *text, size_t size);
/* moves them; false when it could not write, then port.cfg is as it was and later calls of this process do nothing */
bool port_old_files_move(const char *text, size_t size);

#endif
