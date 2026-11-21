/* The mask PSDs read in C: the port of tools/car_mask_from_psd.py and tools/dash_mask_from_psd.py, without python or psd-tools. Both read the layer
 * named "mask" (its own pixels over its own bounds, clipped to the canvas; layers in groups too) and class each texel by its alpha and colour; the
 * rules are the scripts'. Pure and thread-safe. 1 when it reads; 0 with `why` set in English (16-bit, no mask layer, truncated, ...). */
#ifndef TNFS_PSD_MASK_H
#define TNFS_PSD_MASK_H

#include <stddef.h>
#include <stdint.h>

/* A body texture's PSD: `*grey` is width x height bytes of the classes 0 / 128 / 192 / 224 / 240 / 244 / 248 / 252 / 255 (car_mask_codec.h's CAR_MASK_*),
 * malloc'd (free it). Write it with car_mask_write_grey. */
int psd_car_mask(const char *psd_path, uint8_t **grey, uint32_t *w, uint32_t *h, char *why, size_t why_cap);

/* A dash's PSD: `*classes` is width x height bytes of game/race/dash_mask.h's DASH_MASK_* (0 / 64 / 128 / 160 / 192 / 255), with the optional "limpia" or
 * "wipers" layer folded in as the scripts do (write it with dash_mask_write). `color_rgba` may be NULL; otherwise it gets the "color" or "colour" layer as
 * width x height x 4 bytes (the layer's RGB, alpha 255 where the layer's alpha is 128 or more, else 0), or NULL when the PSD has no such layer. Both
 * buffers are malloc'd. */
int psd_dash_mask(const char *psd_path, uint8_t **classes, uint8_t **color_rgba, uint32_t *w, uint32_t *h, char *why, size_t why_cap);

/* A track texture's PSD (tools/track_mask_from_psd.py): the layer "mask"; `*marks` is width x height bytes, TRACK_MASK_NONE / TRACK_MASK_REFLECTIVE / TRACK_MASK_LAMP. A texel
 * is a lamp's mark when it is opaque enough (alpha 128 or more) and yellow (red and green 192 or more, blue 64 or less), else reflective when alpha is 128 or more and its brightest
 * channel 64 or more. `*has_lamp` is 1 when any texel is a lamp (the script writes the PNG as RGBA then, grey otherwise). */
enum { TRACK_MASK_NONE = 0, TRACK_MASK_REFLECTIVE = 1, TRACK_MASK_LAMP = 2 };
int psd_track_mask(const char *psd_path, uint8_t **marks, int *has_lamp, uint32_t *w, uint32_t *h, char *why, size_t why_cap);
int psd_track_mask_bytes(const uint8_t *bytes, size_t len, uint8_t **marks, int *has_lamp, uint32_t *w, uint32_t *h, char *why, size_t why_cap);

/* The same over the bytes of a PSD already in memory (what the tests use). */
int psd_car_mask_bytes(const uint8_t *bytes, size_t len, uint8_t **grey, uint32_t *w, uint32_t *h, char *why, size_t why_cap);
int psd_dash_mask_bytes(const uint8_t *bytes, size_t len, uint8_t **classes, uint8_t **color_rgba, uint32_t *w, uint32_t *h, char *why, size_t why_cap);

#endif
