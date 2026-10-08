/* Game clock. The original ran a 100 Hz multimedia timer (0048c60c, called
 * with 100 from 0048946c) that incremented the tick counter 0052eebc. */
#ifndef TNFS_TIMER_H
#define TNFS_TIMER_H

#include <stdint.h>

enum { TIMER_HZ = 100 };

void timer_init(void);                 /* 0048c60c */
uint32_t timer_ticks(void);            /* 0052eebc */
void timer_set_deadline(int ticks);    /* 0047334f */
int timer_deadline_passed(void);       /* 0047336d */

#endif
