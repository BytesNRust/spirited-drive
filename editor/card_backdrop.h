/* The backdrop of the card photo (the Model window's «Use as card photo» and Card view): what the car stands in front of. A plain colour, a gradient from the top
 * colour to the bottom one, or the person's own picture (PNG) fitted to the card's size, covering it and centred. Always opaque: card.png has no see-through. */
#ifndef TNFS_TRACK_EDITOR_CARD_BACKDROP_H
#define TNFS_TRACK_EDITOR_CARD_BACKDROP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CARD_BACKDROP_PLAIN = 0, CARD_BACKDROP_GRADIENT, CARD_BACKDROP_FILE } CardBackdropKind;

enum { CARD_BACKDROP_PATH = 4096 };

typedef struct {
    int kind;                       /* CardBackdropKind */
    uint8_t top[3], bottom[3];      /* PLAIN: `top`; GRADIENT: top to bottom; FILE: `top` is what shows through a transparent picture */
    char file[CARD_BACKDROP_PATH];  /* FILE: the picture */
} CardBackdrop;

/* a plain dark blue-grey: the car's colours stand out from it */
CardBackdrop card_backdrop_default(void);

/* The backdrop at w x h into `rgba` (w x h, all opaque). 1, or 0 with `why` (English, for the person: the picture that cannot be read, and why). */
int card_backdrop_fill(const CardBackdrop *b, int w, int h, uint8_t *rgba, char *why, size_t why_cap);

#ifdef __cplusplus
}
#endif

#endif
