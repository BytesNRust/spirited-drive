/* port (owner 2026-11-19): the name on a SETUP panel's Color or Skin row: in the row's face (7/8) when it fits the span between the box and the right
 * arrow, else in the next smaller size that does (3/4, then 5/8), and only past that cut with a "." at its end. The longest colour name of the CD's
 * cars, "30th Anniversary Lavender", is 283 px at 7/8 and the span is 218: so a long name is drawn smaller, never over the box or the arrows. */
#ifndef TNFS_SETUP_NAME_H
#define TNFS_SETUP_NAME_H

/* `value` into `out` (size bytes) at the largest size num/den of the three that fits `room` pixels; the text is cut only when the smallest does not
 * fit. Returns the text's width in pixels at that size */
int setup_name_fit(const char *value, int room, char *out, int size, int *num, int *den);

/* the name centred in x0 .. x1 on the row whose middle is `mid` (the row's ink at its own size 7/8): `fits` and drawn in the current style */
void setup_name_draw(const char *value, int x0, int x1, int mid);

#endif
