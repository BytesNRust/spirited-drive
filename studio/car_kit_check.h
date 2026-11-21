/* The report a car kit gets before Build (dep90 item 9): what is wrong that the author can fix, as findings with a severity. The model must build; its wheels
 * are checked (formats/car_check.h); a kit with masks must have one that paints; a mask must have a texture to go over. Nothing is written. */
#ifndef TNFS_STUDIO_CAR_KIT_CHECK_H
#define TNFS_STUDIO_CAR_KIT_CHECK_H

#include "formats/car_check.h"
#include "formats/exchange.h"

/* `kit`'s findings appended to `list`; 0 only when out of memory */
int car_kit_check(const char *kit, CarCheckList *list);

/* tnfs-studio car check KIT_DIR: one line a finding ("error: ...", "warning: ...") on stdout, then the totals; 0 with `err` filled when there is an error */
int car_check_command(int argc, char **argv, ExchangeError *err);

#endif
