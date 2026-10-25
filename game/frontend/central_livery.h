/* port: the car select's Color line (Control Central). With extras psx_liveries on, a line under the player's photo reads
 * "Color: Normal / PS1" with a swatch of the PS1 livery's main colour. M or the pad's X turns it, and the choice is kept
 * per car in extras.cfg (psx_livery_cars); a race takes it for the player's car (track_viewer.c). The PS1 option only
 * shows where the car's PS1 files are in port.cfg's psx_dir; otherwise the line reads "Color: Normal". Flag off, none of
 * this runs and the screen is the exe's. */
#ifndef TNFS_CENTRAL_LIVERY_H
#define TNFS_CENTRAL_LIVERY_H

#include "game/frontend/central_run.h"

void central_livery_init(CentralRun *r);   /* at the screen's start: extras psx_liveries */
void central_livery_turn(CentralRun *r);   /* each turn: the car's line read when the car changes, M or the pad's X */
void central_livery_draw(CentralRun *r);   /* the line, after the banner */

#endif
