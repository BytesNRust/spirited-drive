/* what central_glue.c's pieces share (central_pages.c, central_options_text.c) */
#ifndef TNFS_CENTRAL_SPLIT_H
#define TNFS_CENTRAL_SPLIT_H

#include <stdint.h>

/* the page indicator's triangles; size is 0 when the CD cannot give them */
int32_t central_page_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h);
void central_page_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y);

/* an Options row's text over the screen's own picture, soft like the art */
void central_options_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);

#endif
