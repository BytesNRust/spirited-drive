/* A file of more than 2 GiB (an absurd mod, a DVD-sized image) on every system: its size is read in 64 bits. MinGW's 32-bit stat failed on it (EOVERFLOW), so the file
 * was not there for data_copy_size and data_path_file_size. The file is made sparse (a truncate: no bytes are written; NTFS and APFS allocate nothing for it). */
#include "game/sys/data_copy.h"
#include "game/sys/data_path.h"
#include "rmtree.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

enum { BIG_MIB = 2200 };   /* 2.15 GiB: past 2^31 */

static int make_big(const char *path, int64_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
#ifdef _WIN32   /* _chsize_s writes the zeros; the end of file moved is instant on NTFS */
    const HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    LARGE_INTEGER at;
    at.QuadPart = size;
    const int ok = SetFilePointerEx(h, at, NULL, FILE_BEGIN) && SetEndOfFile(h);
#else
    const int ok = ftruncate(fileno(f), (off_t)size) == 0;
#endif
    fclose(f);
    return ok;
}

int main(void)
{
    char dir[256], path[400];
    snprintf(dir, sizeof dir, "%s/tnfs_large_file_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof path, "%s/BIG.BIN", dir);
    const int64_t big = (int64_t)BIG_MIB * 1024 * 1024;
    if (!make_big(path, big)) {
        fprintf(stderr, "skipped: no room for a sparse 2.15 GiB file here\n");
        test_rmtree(dir);
        return 0;
    }
    char small[400];
    snprintf(small, sizeof small, "%s/SMALL.BIN", dir);
    FILE *f = fopen(small, "wb");
    CHECK(f != NULL);
    if (f != NULL) fwrite("abc", 1, 3, f), fclose(f);

    size_t size = 0;
    CHECK_EQ_INT(data_path_file_size(path, &size), 1);   /* the file is seen, not refused with EOVERFLOW */
    CHECK(size == (size_t)big);
    CHECK(data_copy_size(dir) == big + 3);                /* the folder's total counts it whole */
    CHECK_EQ_INT(data_path_file_size(small, &size), 1);
    CHECK_EQ_INT((int)size, 3);

    CHECK_EQ_INT(test_rmtree(dir), 0);
    return TEST_REPORT();
}
