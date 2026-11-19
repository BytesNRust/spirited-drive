/* port (dep59 showcase, owner 2026-11-19): a car's RECORDS screen, a screen of its own: the car's page background (its tone, its title band with the
 * name), "Best Times" over the table of car_records_view.h in Best Times' rows and letters, the page selector (the standard arrows, the footer's big
 * letters) and DONE on one height at the foot. It fades in from black as the screens of the game do and out to black at DONE; the page it was opened
 * from draws itself again after. Run by the added car's page and by the CD's pages, each giving its palette and its background. */
#ifndef TNFS_CAR_RECORDS_SCREEN_H
#define TNFS_CAR_RECORDS_SCREEN_H

#include <stdint.h>

typedef struct {
    const uint8_t *palette;    /* the page's, 768 bytes, 8 bits a channel */
    const uint8_t *ground;     /* the page's background without its tiles, 640 x 480 indices of the palette, the title band included */
    void (*title)(void *ctx);  /* the page's title drawn over the ground (an added car's name), or NULL when the ground has it */
    void *ctx;
} CarRecordsLook;

/* `out` (640 x 480 indices, holding the page's picture) with its rows from the title band's end down made the clean configuration background
 * (`tint` of the page when not NULL, each shade keeping its brightness), each pixel the nearest of the `usable` colours of `palette` (8 bits a channel) */
void car_records_ground(uint8_t *out, const uint8_t palette[768], const uint8_t usable[256], const uint8_t tint[3]);

/* the screen for the car `id` (the registry's, a stock car's included) until DONE (a click on it, Enter, Escape or the back button); Left and Right
 * (or a click on the selector's arrows) turn the table's pages. The display is left dark. */
void car_records_screen(const char *id, const CarRecordsLook *look);

#endif
