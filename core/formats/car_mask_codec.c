/* See car_mask_codec.h. */
#include "formats/car_mask_codec.h"

#include "formats/png.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, MATTE_FROM = 192, BODY_UNTIL = 224, BLACK_UNTIL = 240, LIGHT_UNTIL = 255, GREY_SPREAD = 16, MAGENTA_FROM = 128, MAGENTA_APART = 32 };

/* the class of a mask colour */
static int class_of(const uint8_t c[3])
{
    if (c[0] == 0 && c[1] == 0 && c[2] == 0) return CAR_MASK_PAINT;
    const int pink = c[0] < c[2] ? c[0] : c[2];   /* the lesser of red and blue: a magenta has both well above green, and near each other */
    if (pink >= MAGENTA_FROM && c[1] * 2 <= pink && abs((int)c[0] - (int)c[2]) <= MAGENTA_APART) return CAR_MASK_BLACK;   /* the PSDs' magenta: a texel painted black */
    if (c[2] > c[0] && c[2] > c[1]) return CAR_MASK_SHINE;
    if (c[1] > c[0] && c[1] > c[2]) return CAR_MASK_BODY;   /* the owner's convention in the PSDs: green is a body texel that keeps its colour */
    const int top = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    const int low = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
    if (top >= MATTE_FROM && top < BODY_UNTIL && top - low <= GREY_SPREAD) return CAR_MASK_BODY;   /* the grey 192 the tools write */
    if (top >= BODY_UNTIL && top < BLACK_UNTIL && top - low <= GREY_SPREAD) return CAR_MASK_BLACK;   /* the grey 224 */
    if (top >= BLACK_UNTIL && top < LIGHT_UNTIL && top - low <= GREY_SPREAD)   /* the greys of the lamps */
        return top < CAR_MASK_POSITION ? CAR_MASK_BRAKE : top < CAR_MASK_HEADLIGHT ? CAR_MASK_POSITION : top < CAR_MASK_REVERSE ? CAR_MASK_HEADLIGHT : CAR_MASK_REVERSE;
    return top >= MATTE_FROM ? CAR_MASK_MATTE : CAR_MASK_SHINE;
}

int car_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out)
{
    if (png == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            out[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)class_of(c);
        }
    }
    png_free(img.pixels);
    return 1;
}

int car_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len)
{
    if (mask == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *indices = malloc(count);
    if (indices == NULL) return 0;
    for (size_t i = 0; i < count; i++) indices[i] = mask[i] == 0 ? 0 : mask[i] == CAR_MASK_SHINE ? 1 : mask[i] == CAR_MASK_BODY ? 3 : mask[i] == CAR_MASK_BLACK ? 4 : mask[i] == CAR_MASK_BRAKE ? 5 : mask[i] == CAR_MASK_POSITION ? 6 : mask[i] == CAR_MASK_HEADLIGHT ? 7 : mask[i] == CAR_MASK_REVERSE ? 8 : 2;
    uint8_t palette[768] = { 0 };
    palette[3] = palette[4] = palette[5] = CAR_MASK_SHINE;
    palette[6] = palette[7] = palette[8] = CAR_MASK_MATTE;
    palette[9] = palette[10] = palette[11] = CAR_MASK_BODY;
    palette[12] = palette[13] = palette[14] = CAR_MASK_BLACK;
    palette[15] = palette[16] = palette[17] = CAR_MASK_BRAKE;
    palette[18] = palette[19] = palette[20] = CAR_MASK_POSITION;
    palette[21] = palette[22] = palette[23] = CAR_MASK_HEADLIGHT;
    palette[24] = palette[25] = palette[26] = CAR_MASK_REVERSE;
    const PngResult r = png_write_indexed(indices, (uint32_t)width, (uint32_t)height, palette, out, len);
    free(indices);
    return r == PNG_OK;
}

int car_mask_write_grey(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len)
{
    if (mask == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *grey = malloc(count);
    if (grey == NULL) return 0;
    for (size_t i = 0; i < count; i++) {
        const uint8_t m = mask[i];
        grey[i] = m == CAR_MASK_PAINT || m == CAR_MASK_SHINE || m == CAR_MASK_BODY || m == CAR_MASK_BLACK || m == CAR_MASK_BRAKE || m == CAR_MASK_POSITION ||
                  m == CAR_MASK_HEADLIGHT || m == CAR_MASK_REVERSE ? m : CAR_MASK_MATTE;
    }
    const PngResult r = png_write_grey(grey, (uint32_t)width, (uint32_t)height, out, len);
    free(grey);
    return r == PNG_OK;
}

enum { LAMP_FROM = 192, LAMP_OFF = 64, ORANGE_FROM = 96, ORANGE_TO = 176, DARK_UNDER = 64, ALPHA_FROM = 128 };

/* the lamp class of a layer colour: red brake, orange position, yellow headlight, cyan reversing light; 0 for none of them (the tool's lamp_class) */
static int lamp_class(int r, int g, int b)
{
    if (r <= LAMP_OFF && g >= LAMP_FROM && b >= LAMP_FROM) return CAR_MASK_REVERSE;
    if (r < LAMP_FROM || b > LAMP_OFF) return 0;
    if (g <= LAMP_OFF) return CAR_MASK_BRAKE;
    if (g >= ORANGE_FROM && g <= ORANGE_TO) return CAR_MASK_POSITION;
    if (g >= LAMP_FROM) return CAR_MASK_HEADLIGHT;
    return 0;
}

int car_mask_class_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a < ALPHA_FROM) return CAR_MASK_PAINT;
    const int lamp = lamp_class(r, g, b);
    if (lamp) return lamp;
    const int pink = r < b ? r : b;
    if (pink >= MAGENTA_FROM && g * 2 <= pink && abs((int)r - (int)b) <= MAGENTA_APART) return CAR_MASK_BLACK;
    if (b > r && b > g) return CAR_MASK_SHINE;
    if (g > r && g > b) return CAR_MASK_BODY;   /* the one strictly greatest channel is green */
    const int top = r > g ? (r > b ? r : b) : (g > b ? g : b);
    return top >= DARK_UNDER ? CAR_MASK_MATTE : CAR_MASK_PAINT;
}
