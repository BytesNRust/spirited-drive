/* dep59 step 4: Restomod's stores at their first use (records_glue_load): the standard rows go to their cars' tables once, saved at once in
 * race_records_cars2.dat with the flag that says so; race_records.dat is not written. (A race_records.dat that could not be read copies
 * nothing and writes nothing: records_glue_load's own test.) */
#include "game/frontend/records_glue.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_added.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { FILES = 4, TR1 = 3 };
static struct {
    char name[40];
    uint8_t *data;
    size_t size;
} files[FILES];
static int writes_std, writes_cars;

static int slot_of(const char *name, int make)
{
    for (int i = 0; i < FILES; i++)
        if (files[i].data != NULL && strcmp(files[i].name, name) == 0) return i;
    if (!make) return -1;
    for (int i = 0; i < FILES; i++)
        if (files[i].data == NULL) return snprintf(files[i].name, sizeof files[i].name, "%s", name), i;
    return -1;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const int i = slot_of(name, 0);
    if (i < 0) return NULL;
    uint8_t *copy = malloc(files[i].size ? files[i].size : 1);
    memcpy(copy, files[i].data, files[i].size);
    *size = files[i].size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    const int i = slot_of(name, 1);
    if (i < 0) return false;
    free(files[i].data);
    files[i].data = malloc(size ? size : 1);
    memcpy(files[i].data, data, size);
    files[i].size = size;
    writes_std += strcmp(name, "race_records.dat") == 0;
    writes_cars += strcmp(name, "race_records_cars2.dat") == 0;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name) { return slot_of(name, 0) >= 0; }
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, *size = 0; return NULL; }

int main(void)
{
    setenv("TNFS_RECORDS_UNIFIED", "1", 1);
    /* a race_records.dat with a supra's row */
    RecordRow *row = &race_records_make(TR1, LAYOUT_NORMAL, 0, 4)->totals[0];
    const int32_t time = 1234, car = 2, set = 1;
    memcpy(row->row + RECORD_ROW_TIME, &time, 4), memcpy(row->row + RECORD_ROW_SET, &set, 4);
    memcpy(row->row + RECORD_ROW_CAR, &car, 4), memcpy(row->row + RECORD_ROW_CAR_B, &car, 4);
    CHECK(race_records_save());
    race_records_reset();
    writes_std = 0;

    static uint8_t image[RECORD_CHECK_IMAGE];
    records_glue_load(image);
    CHECK_EQ_INT(writes_std, 0);
    CHECK_EQ_INT(writes_cars, 1);
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(race_records_cars_imported(), 1);
    const RecordTable *supra = race_records_added_find(TR1, LAYOUT_NORMAL, 0, 4, "supra");
    int32_t got = 0;
    if (supra != NULL) memcpy(&got, supra->totals[0].row + RECORD_ROW_TIME, 4);
    CHECK_EQ_INT(got, 1234);
    records_glue_load(image);   /* once a session */
    CHECK_EQ_INT(writes_cars, 1);
    race_records_reset();
    race_records_added_reset();
    for (int i = 0; i < FILES; i++) free(files[i].data);
    return TEST_REPORT();
}
