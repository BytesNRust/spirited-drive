/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the full-screen key (platform/fullscreen_key.h): Graphics' Full screen row, from the keyboard. */
#ifndef TNFS_FULLSCREEN_TOGGLE_H
#define TNFS_FULLSCREEN_TOGGLE_H

/* The full-screen setting turned over and kept in port.cfg (as the Graphics screen keeps it, in every mode), and the window takes it now (platform_reconfigure, which waits for the change to end). The format is left as it is: it is not
 * changed in the middle of a race, and Graphics' own Full screen row is where the desktop's shape is taken. */
void fullscreen_toggle_now(void);

#endif
