/* port: a key tapped between two polls of the keyboard is still seen by the sim. SDL's keyboard state is the keys as of the last pump of the
 * events: a press and release that both arrive in one pump (a long frame under load) leave it up, and the press is lost. The latch keeps a
 * pressed key down for KEY_LATCH_STEPS sim steps (or KEY_LATCH_PUMPS pumps) from its press, whatever the state says; a key still held reads down by the state. */
#ifndef TNFS_KEY_LATCH_H
#define TNFS_KEY_LATCH_H

#include <stdbool.h>

enum { KEY_LATCH_KEYS = 512, KEY_LATCH_STEPS = 2, KEY_LATCH_PUMPS = 3 };   /* SDL's scancodes are under 512; the exe polls the keys on every second step */

/* a key went down (its event): it reads down for the next KEY_LATCH_STEPS steps */
void key_latch_press(int key);
/* the events were pumped again: a latch older than KEY_LATCH_PUMPS pumps lets go whatever the steps (a screen that reads no sim steps) */
void key_latch_pump(void);
/* the key is latched down now */
bool key_latch_down(int key);
/* a sim step has read the keys: every latch has one step less */
void key_latch_step(void);
/* every latch cleared (a window that lost focus sees no key) */
void key_latch_clear(void);

#endif
