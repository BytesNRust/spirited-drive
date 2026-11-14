/* Restomod's store at its first use (records_glue_load): a race_records.dat that is there but cannot be read (locked, an I/O error) is
 * not made again from config.dat over it; saving is held for the session, and the file is never written. */
#include "game/frontend/records_glue.h"
#include "game/sys/race_records.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

static int writes;

uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, *size = 0; return NULL; }   /* every read fails */
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    writes++;
    return true;
}
void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name) { return strcmp(name, "race_records.dat") == 0; }   /* but the file is there */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, *size = 0; return NULL; }

int main(void)
{
    static uint8_t image[RECORD_CHECK_IMAGE];
    setenv("TNFS_RECORDS_UNIFIED", "1", 1);
    records_glue_load(image);
    CHECK_EQ_INT(writes, 0);
    CHECK(race_records_find(3, LAYOUT_NORMAL, 0, 4) == NULL);   /* no import */
    CHECK(race_records_make(3, LAYOUT_NORMAL, 0, 4) != NULL);
    CHECK(!race_records_save());
    CHECK_EQ_INT(writes, 0);
    race_records_reset();
    return TEST_REPORT();
}
