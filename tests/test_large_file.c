/* A file of more than 2 GiB (an absurd mod, a DVD-sized image) on every system: its size is read in 64 bits. MinGW's 32-bit stat failed on it (EOVERFLOW), so the file
 * was not there for data_copy_size and data_path_file_size. The file is made sparse: on Windows it is marked sparse (FSCTL_SET_SPARSE) before its end is moved, or NTFS
 * reserves the 2.15 GiB; elsewhere a truncate (APFS and ext4 allocate nothing; a file system without sparse files, FAT or HFS+, writes it). The folder is removed at exit,
 * also when a check aborts the run, and the test has a TIMEOUT in CMake. */
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
#include <winioctl.h>
#endif

enum { BIG_MIB = 2200 };   /* 2.15 GiB: past 2^31 */

static int make_big(const char *path, int64_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
#ifdef _WIN32   /* _chsize_s writes the zeros; the end of file moved is instant on NTFS */
    const HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    LARGE_INTEGER at;
    DWORD returned = 0;
    at.QuadPart = size;
    const int ok = DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &returned, NULL) && SetFilePointerEx(h, at, NULL, FILE_BEGIN) && SetEndOfFile(h);
#else
    const int ok = ftruncate(fileno(f), (off_t)size) == 0;
#endif
    fclose(f);
    return ok;
}

static char dir[256];

static void remove_folder(void)
{
    if (dir[0] != '\0') test_rmtree(dir);
}

int main(void)
{
    char path[400];
    snprintf(dir, sizeof dir, "%s/tnfs_large_file_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    CHECK(mkdtemp(dir) != NULL);
    atexit(remove_folder);   /* a failed check or an abort from the code under test must not leave the big file behind */
    snprintf(path, sizeof path, "%s/BIG.BIN", dir);
    const int64_t big = (int64_t)BIG_MIB * 1024 * 1024;
    if (!make_big(path, big)) {
        fprintf(stderr, "skipped: no room for a sparse 2.15 GiB file here\n");
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
    dir[0] = '\0';
    return TEST_REPORT();
}
