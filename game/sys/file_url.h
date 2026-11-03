/* port: a file:// URL from a path, for opening a folder in the platform's file browser (platform_open_user_dir). */
#ifndef TNFS_FILE_URL_H
#define TNFS_FILE_URL_H

#include <stddef.h>

/* "file://" and the absolute `path` with every byte but letters, digits and -._~/ written as %XX (a space, a comma, a parenthesis or a non-ASCII byte in the path would
 * make the URL invalid to the OS). Returns the length written, or 0 when the path is not absolute or the result does not fit `cap` (with its end). */
size_t file_url_from_path(const char *path, char *out, size_t cap);

#endif
