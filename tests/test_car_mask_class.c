/* The colour classifier of a mask layer (car_mask_class_rgba) against the golden table of the PSD tool's rules (tests/golden/car_mask_class.txt,
 * made by tests/gen_car_mask_class_golden.py without Pillow): every colour of the grid, the alpha threshold, the rules the table's header lists. */
#include "formats/car_mask_codec.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int class_of_letter(char c)
{
    switch (c) {
    case '.': return CAR_MASK_PAINT;
    case 'S': return CAR_MASK_SHINE;
    case 'B': return CAR_MASK_BODY;
    case 'K': return CAR_MASK_BLACK;
    case 'R': return CAR_MASK_BRAKE;
    case 'P': return CAR_MASK_POSITION;
    case 'H': return CAR_MASK_HEADLIGHT;
    case 'V': return CAR_MASK_REVERSE;
    case 'M': return CAR_MASK_MATTE;
    default: return -1;
    }
}

static void test_table(void)
{
    FILE *f = fopen(TNFS_SOURCE_DIR "/tests/golden/car_mask_class.txt", "r");
    CHECK(f != NULL);
    if (f == NULL) return;
    char line[512];
    int blues[64], nblue = 0, rows = 0, bad = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        if (line[0] == '#') continue;
        if (strncmp(line, "blue ", 5) == 0) {
            char *p = line + 5, *end = p;
            for (long v = strtol(p, &end, 10); end != p && nblue < 64; v = strtol(p, &end, 10)) blues[nblue++] = (int)v, p = end;
            continue;
        }
        int r, g, used;
        if (sscanf(line, "%d %d %n", &r, &g, &used) < 2) continue;
        rows++;
        for (int i = 0; i < nblue; i++) {
            const int want = class_of_letter(line[used + i]);
            const int got = car_mask_class_rgba((uint8_t)r, (uint8_t)g, (uint8_t)blues[i], 255);
            if (got != want && bad++ < 8) printf("rgb %d %d %d: got %d, want %d\n", r, g, blues[i], got, want);
        }
    }
    fclose(f);
    CHECK(nblue > 30);
    CHECK(rows == nblue * nblue);
    CHECK(bad == 0);
}

static void test_rules(void)
{
    CHECK(car_mask_class_rgba(255, 0, 0, 127) == CAR_MASK_PAINT);    /* alpha under 128 paints, whatever the colour */
    CHECK(car_mask_class_rgba(255, 0, 0, 128) == CAR_MASK_BRAKE);
    CHECK(car_mask_class_rgba(0, 255, 255, 255) == CAR_MASK_REVERSE);   /* cyan before blue and green */
    CHECK(car_mask_class_rgba(255, 0, 255, 255) == CAR_MASK_BLACK);
    CHECK(car_mask_class_rgba(0, 200, 0, 255) == CAR_MASK_BODY);        /* the owner's green: 192 */
    CHECK(car_mask_class_rgba(0, 0, 200, 255) == CAR_MASK_SHINE);
    CHECK(car_mask_class_rgba(30, 30, 30, 255) == CAR_MASK_PAINT);      /* dark opaque is paint */
    CHECK(car_mask_class_rgba(200, 200, 200, 255) == CAR_MASK_MATTE);
}

int main(void)
{
    test_table();
    test_rules();
    return TEST_REPORT();
}
