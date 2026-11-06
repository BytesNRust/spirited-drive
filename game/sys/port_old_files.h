/* port (one time): port.cfg used to hold every setting and extras.cfg the switches. At startup a port.cfg that still carries settings moves them to the mode's file. Copies
 * stay as port.cfg.bak and extras.cfg.bak (one already there is kept), port.cfg is left with profile= and psx_dir=, and extras.cfg goes. A named mode keeps its name and what it
 * left alone that stays free in it; custom, or no profile line, becomes Restomod, the only mode where all of it stays free, and every key it set stays set even at the table's
 * value. A key it did not set takes Restomod's. Goes away once no old files are left. */
#ifndef TNFS_PORT_OLD_FILES_H
#define TNFS_PORT_OLD_FILES_H

/* once, after the user folder is known, before any setting is read and before there are threads. When it cannot write, port.cfg is as it was and the next run tries again */
void port_old_files_migrate(void);

#endif
