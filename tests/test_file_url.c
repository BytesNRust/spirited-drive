/* The file:// URL of a folder (file_url.c): the characters a path can have that a URL cannot. */
#include "game/sys/file_url.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char url[128];
    CHECK(file_url_from_path("/home/x/Library/Application Support/tnfs/", url, sizeof url) > 0);
    CHECK(strcmp(url, "file:///home/x/Library/Application%20Support/tnfs/") == 0);
    CHECK(file_url_from_path("/a, b (c)/\xc3\xa9.log", url, sizeof url) > 0);
    CHECK(strcmp(url, "file:///a%2C%20b%20%28c%29/%C3%A9.log") == 0);
    CHECK_EQ_INT((int)file_url_from_path("/a-b_c.d~e/", url, sizeof url), (int)strlen("file:///a-b_c.d~e/"));
    CHECK_EQ_INT((int)file_url_from_path("relative/dir", url, sizeof url), 0);   /* not absolute */
    CHECK_EQ_INT((int)file_url_from_path(NULL, url, sizeof url), 0);
    CHECK_EQ_INT((int)file_url_from_path("/a b", url, 10), 0);                  /* "file:///a%20b" does not fit 10 */
    CHECK_EQ_INT((int)file_url_from_path("/ab", url, 11), 10);                  /* exactly: "file:///ab" and the end */
    CHECK_EQ_INT((int)file_url_from_path("/ab", url, 10), 0);
    /* Windows: SDL's pref path is C:\Player\...\Spirited Drive\ */
    CHECK(file_url_from_path("C:\\Player\\Zo\xc3\xab\\AppData\\Roaming\\Spirited Drive\\", url, sizeof url) > 0);
    CHECK(strcmp(url, "file:///C:/Player/Zo%C3%AB/AppData/Roaming/Spirited%20Drive/") == 0);
    CHECK(file_url_from_path("d:/Games/NFS SE", url, sizeof url) > 0);
    CHECK(strcmp(url, "file:///d:/Games/NFS%20SE") == 0);
    CHECK(file_url_from_path("\\\\nas\\share\\my logs\\", url, sizeof url) > 0);
    CHECK(strcmp(url, "file://nas/share/my%20logs/") == 0);
    CHECK_EQ_INT((int)file_url_from_path("C:", url, sizeof url), 0);                 /* no separator: the drive's current folder, not absolute */
    CHECK_EQ_INT((int)file_url_from_path("C:relative\\x", url, sizeof url), 0);
    CHECK_EQ_INT((int)file_url_from_path("\\\\", url, sizeof url), 0);
    CHECK_EQ_INT((int)file_url_from_path("C:\\a b", url, 14), 0);                   /* "file:///C:/a%20b" does not fit */
    CHECK(file_url_from_path("/a\\b:c", url, sizeof url) > 0);                      /* on POSIX a backslash and a colon are bytes of a name */
    CHECK(strcmp(url, "file:///a%5Cb%3Ac") == 0);
    return TEST_REPORT();
}
