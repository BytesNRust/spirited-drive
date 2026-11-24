/* The Track light panel's strip bar: the 11 points of a row as the road is seen (the left's outer point first, the centre in the middle), a click on a cell turns that point on or off (any set of points, not only a run),
 * and the buttons All / Left / Right / Road only set the picked points (then each can be changed one by one). The mask is light_strip.h's. */
#ifndef TNFS_EDITOR_LIGHT_STRIP_BAR_H
#define TNFS_EDITOR_LIGHT_STRIP_BAR_H

/* Draws the bar and the shortcuts; *mask is the picked strip. Returns true when the pick changed this frame. `id` must be unique in the window. The caption says the strip as the file writes it. */
bool light_strip_bar(const char *id, unsigned *mask);

#endif
