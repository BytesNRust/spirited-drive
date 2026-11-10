/* 00429278's record defaults, read from the exe (tools/gen_config_records.py). */
#ifndef TNFS_CONFIG_RECORDS_H
#define TNFS_CONFIG_RECORDS_H

#include <stdint.h>

enum {
    CONFIG_RECORD_FIRST = 0x004b9638,  /* the address of row 0, for the rows' own addresses */
    CONFIG_RECORD_ROW_SIZE = 24,       /* six pointers */
    CONFIG_RECORD_ROW_COUNT = (0x004bad00 - 0x004b9638) / 24
};

typedef struct {
    const char *name;
    int32_t number[5];
} ConfigRecordRow;

extern ConfigRecordRow CONFIG_RECORD_ROWS[CONFIG_RECORD_ROW_COUNT];
extern const char *CONFIG_RECORD_NAMES[3];

#endif
