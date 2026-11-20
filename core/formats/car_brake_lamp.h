/* A car's brake lamp as the race colours it: the exe's own mechanism, not the masks. The CD's car textures paint their brake lights in palette entry 0xfe (a
 * phosphor green in the files); the race maps that entry to the slot's own screen entry (game/race/race_palette.c race_palette_remap_bank, 0x47 for the
 * player) and writes it every frame with the car model's lamp colour (game/race/car_lights.c, 00458d4c): the row of the model off, 13 rows on while the brake
 * is above 10. The colour is written after the tint, so neither the time of day nor the light at the car reaches it. The masks' lamp classes
 * (formats/lamp_emissive.h) are the port's, on top of this. */
#ifndef TNFS_CAR_BRAKE_LAMP_H
#define TNFS_CAR_BRAKE_LAMP_H

#include <stdint.h>

enum {
    CAR_BRAKE_LAMP_ENTRY = 0xfe,   /* the bank's entry the race paints as the lamp */
    CAR_BRAKE_LAMP_MODELS = 13,    /* the rows of the unlit colours; the lit ones follow */
    CAR_BRAKE_LAMP_ROWS = 2 * CAR_BRAKE_LAMP_MODELS,
    CAR_BRAKE_LAMP_STANDARD = 0    /* an added car's row (dep59 3b): the red ten of the twelve originals share */
};

/* 004c578c: 6 bits a channel, a row of three a model (game/race/car_load.c car_model_names' order), the lit rows CAR_BRAKE_LAMP_MODELS on */
extern const uint8_t car_brake_lamp_rows[CAR_BRAKE_LAMP_ROWS * 3];

/* The row of the original car whose stem is `stem` (any case: "P911", "TSupra"), CAR_BRAKE_LAMP_STANDARD for any other (an added car) */
int car_brake_lamp_row(const char *stem);

/* The lamp's colour (6 bits a channel) for row `row`, lit or off; the standard row's for a row out of the table */
void car_brake_lamp_colour(int row, int lit, uint8_t six[3]);

#endif
