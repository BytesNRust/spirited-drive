#include "shortcut_keys.h"

#include <stdio.h>

const char *shortcut_mod_name(int mac)
{
    return mac ? "Cmd" : "Ctrl";
}

int shortcut_label(char *out, size_t cap, int mac, const char *keys)
{
    if (out == NULL || cap == 0) return 0;
    const int n = snprintf(out, cap, "%s+%s", shortcut_mod_name(mac), keys != NULL ? keys : "");
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}
