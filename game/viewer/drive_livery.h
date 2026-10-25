/* port: a rival's PS1 livery at load (extras.cfg psx_liveries, car_livery.h). The PS1 files are the user's own, read from
 * port.cfg's psx_dir (a folder of the CD's Z<CAR>.VSM and Z<CAR>.WSM); with the key unset or a file missing the car keeps
 * its normal colours. */
#ifndef TNFS_DRIVE_LIVERY_H
#define TNFS_DRIVE_LIVERY_H

#include <stddef.h>
#include <stdint.h>

/* Remaps the car's CFM in place (the bytes the loader reads, before it loads them) to its PS1 alternate livery: the map
 * is built from psx_dir's Z<STEM>.VSM and .WSM (STEM as the PC names the car, "P911", "TSupra") plus the car's table of
 * body palette entries (drive_livery.c), and applied to the palettes of the CFM's high and low detail banks and its
 * '!xxx' colours. The CFM buffer is the caller's, so the change is this car's alone. Returns the number of colours
 * mapped; 0 when a file is missing or there is nothing to map, and then the CFM is left as it was. */
int drive_livery_car(const char *psx_dir, const char *stem, uint8_t *cfm, size_t size);

#endif
