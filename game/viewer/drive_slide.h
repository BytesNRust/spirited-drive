/* The loading pictures (004571e8, after its pick: race_loading_pick): "<00524558><004c5660[car]>s<n>.qfs", 00524558
 * the slides' directory 0042a188 sets ("simdata\slides\", German "simdata\gslides\"), read whole (00476268); its
 * "SCRN" shape (004732dd) with the palette after the shape's block ((*shape >> 8) + 0x10) put to the screen
 * (00476460), the shape drawn (00473780) and shown (00473614). */
#ifndef TNFS_DRIVE_SLIDE_H
#define TNFS_DRIVE_SLIDE_H

#include "formats/fsh.h"

#include <stddef.h>
#include <stdint.h>

/* the file's name in `dir` (resource_dir(PATH_SLIDES)); "" for a car past 004c5660's 12 */
void drive_slide_name(char *out, size_t size, const char *dir, int32_t car, int32_t picture);
/* the "SCRN" shape and its palette's 768 bytes; 0 when either is missing or runs past the file */
int drive_slide_parts(const uint8_t *data, size_t size, FshEntry *shape, const uint8_t **rgb);
/* 00476460's colours: each 6-bit value v as v * 4 | v >> 4 (63 -> 255) */
void drive_slide_palette(const uint8_t *six, uint8_t rgb[768]);
/* the picture on the front end's screen (video.h): nothing when its file does not load */
void drive_slide_show(int32_t car, int32_t picture);

#endif
