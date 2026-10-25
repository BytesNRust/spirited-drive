/* port: the render band a thread is running (bands.h), apart from bands.c so a stage that reads it links without the
 * platform's threads. */
#include "game/sys/bands.h"

static BANDS_THREAD_LOCAL int band_index;

int bands_index(void) { return band_index; }

void bands_set_index(int index) { band_index = index; }
