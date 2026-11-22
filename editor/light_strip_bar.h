/* The Track light panel's strip bar: the 11 points of a row as the road is seen (the left's outer point first, the centre in the middle), a drag over cells picks a strip,
 * and the buttons All / Left / Right / Road fill it. The mask is light_strip.h's. */
#ifndef TNFS_EDITOR_LIGHT_STRIP_BAR_H
#define TNFS_EDITOR_LIGHT_STRIP_BAR_H

/* Draws the bar and the shortcuts; *mask is the picked strip. Returns true when the pick changed this frame. `id` must be unique in the window. The bar says in its
 * caption when the pick cannot be one line of a file (light_strip_text). */
bool light_strip_bar(const char *id, unsigned *mask);

#endif
