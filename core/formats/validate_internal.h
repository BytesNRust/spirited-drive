/* Internal: the per-format checkers behind validate_file. Each appends to the report and returns nothing;
 * `name` is how the file is shown in findings. */
#ifndef TNFS_VALIDATE_INTERNAL_H
#define TNFS_VALIDATE_INTERNAL_H

#include "formats/validate.h"

int validate_has_extension(const char *name, const char *ext);
const char *validate_base_name(const char *name);

void validate_tri(const uint8_t *data, size_t size, const char *name, Report *r);
void validate_track_fam(const uint8_t *data, size_t size, const char *name, Report *r);
void validate_car_cfm(const uint8_t *data, size_t size, const char *name, Report *r);
void validate_shpi(const uint8_t *data, size_t size, const char *name, Report *r);
void validate_bnk(const uint8_t *data, size_t size, const char *name, Report *r);
void validate_info(const uint8_t *data, size_t size, const char *name, Report *r);

#endif
