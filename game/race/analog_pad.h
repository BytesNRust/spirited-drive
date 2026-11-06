/* port (analog_pad): which of the player's control words is a pad's analog deflection and which is a key. The exe's word has 6 bits for the steering and 6 for the pedal; the keys write only a few
 * values of them (input_read.c: steering 0, the centre 0x20 and 0x3c; the pedal at rest 0x1e, 5 for the accelerate key and 0x3c for the brake key), while a pad's axis writes any value between. With the flag
 * the race follows the exe's analog steering and pedals (car_control.c's analog_steering and analog_pedals) for a word that is not one of the key values, and its key paths for the rest, so an action bound to
 * an axis has its travel and one bound to a key or button keeps its full lock and full pedal, and a keyboard alone plays exactly as before. */
#ifndef TNFS_ANALOG_PAD_H
#define TNFS_ANALOG_PAD_H

#include <stdint.h>

/* 1 when `on` and the steering field of `input` is a pad's deflection (not a key's word). */
int analog_pad_steering(int on, uint32_t input);

/* 1 when `on` and the pedal field of `input` is a pad's deflection (not a key's word, and not the rest). */
int analog_pad_pedal(int on, uint32_t input);

#endif
