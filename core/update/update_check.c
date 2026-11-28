/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "update/update_check.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int update_parse_latest_tag(const char *json, char *out, size_t cap)
{
    if (cap == 0) return 0;
    out[0] = '\0';
    if (!json) return 0;

    const char *key = strstr(json, "\"tag_name\"");
    if (!key) return 0;
    const char *p = key + strlen("\"tag_name\"");
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != ':') return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != '"') return 0;
    if (*p == 'v' || *p == 'V') p++;

    size_t n = 0;
    while (p[n] && p[n] != '"') {
        unsigned char c = (unsigned char)p[n];
        /* a version tag is short and plain: anything else is not one */
        if (!(isalnum(c) || c == '.' || c == '-' || c == '+')) return 0;
        n++;
    }
    if (p[n] != '"' || n == 0 || n + 1 > cap) return 0;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

typedef struct {
    long core[3];
    char pre[64];          /* the pre-release part, empty for a release */
} Version;

static int parse_version(const char *s, Version *v)
{
    memset(v, 0, sizeof *v);
    if (!s) return 0;
    if (*s == 'v' || *s == 'V') s++;
    for (int i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)*s)) return 0;
        char *end;
        v->core[i] = strtol(s, &end, 10);
        s = end;
        if (i < 2) {
            if (*s != '.') return 0;
            s++;
        }
    }
    if (*s == '-') {
        s++;
        size_t n = strcspn(s, "+");
        if (n == 0 || n >= sizeof v->pre) return 0;
        memcpy(v->pre, s, n);
        v->pre[n] = '\0';
        s += n;
    }
    return *s == '\0' || *s == '+';
}

static int is_number(const char *s, size_t n)
{
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++)
        if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

/* SemVer pre-release precedence: dot-separated identifiers, numbers numerically and before words. */
static int compare_pre(const char *a, const char *b)
{
    if (!*a && !*b) return 0;
    if (!*a) return 1;     /* a release is newer than its pre-releases */
    if (!*b) return -1;
    while (*a && *b) {
        size_t na = strcspn(a, "."), nb = strcspn(b, ".");
        int da = is_number(a, na), db = is_number(b, nb);
        int c;
        if (da && db) {
            long x = strtol(a, NULL, 10), y = strtol(b, NULL, 10);
            c = (x > y) - (x < y);
        } else if (da != db) {
            c = da ? -1 : 1;
        } else {
            size_t m = na < nb ? na : nb;
            c = strncmp(a, b, m);
            if (c == 0) c = (na > nb) - (na < nb);
            c = (c > 0) - (c < 0);
        }
        if (c) return c;
        a += na;
        b += nb;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return (*a != '\0') - (*b != '\0');
}

int update_version_compare(const char *a, const char *b)
{
    Version va, vb;
    if (!parse_version(a, &va) || !parse_version(b, &vb)) return 0;
    for (int i = 0; i < 3; i++)
        if (va.core[i] != vb.core[i]) return va.core[i] < vb.core[i] ? -1 : 1;
    return compare_pre(va.pre, vb.pre);
}

int update_is_dev_build(const char *version)
{
    Version v;
    if (!parse_version(version, &v)) return 1;   /* unreadable: never nag */
    return strncmp(v.pre, "dev", 3) == 0;
}

enum { API_VERSION_MAX = 32 };

/* MAJOR.MINOR.PATCH[-pre]: three numbers, then optionally '-' and letters, digits, '.' and '-' */
static int is_api_version(const char *s, size_t n)
{
    size_t i = 0;
    if (n == 0 || n > API_VERSION_MAX) return 0;
    for (int part = 0; part < 3; part++) {
        const size_t start = i;
        while (i < n && isdigit((unsigned char)s[i])) i++;
        if (i == start) return 0;
        if (part < 2) {
            if (i >= n || s[i] != '.') return 0;
            i++;
        }
    }
    if (i == n) return 1;
    if (s[i++] != '-' || i == n) return 0;
    for (; i < n; i++)
        if (!(isalnum((unsigned char)s[i]) || s[i] == '.' || s[i] == '-')) return 0;
    return 1;
}

int update_parse_api_latest(const char *json, char *out, size_t cap)
{
    if (cap == 0) return 0;
    out[0] = '\0';
    if (!json) return 0;
    const char *key = strstr(json, "\"latest\"");
    if (!key) return 0;
    const char *p = key + strlen("\"latest\"");
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != ':') return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != '"') return 0;   /* null, a number, ... */
    const size_t n = strcspn(p, "\"");
    if (p[n] != '"' || !is_api_version(p, n) || n + 1 > cap) return 0;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

int update_api_version(const char *version, char *out, size_t cap)
{
    if (cap == 0) return 0;
    out[0] = '\0';
    if (!version) return 0;
    const size_t n = strcspn(version, "+");
    if (!is_api_version(version, n) || n + 1 > cap) return 0;
    memcpy(out, version, n);
    out[n] = '\0';
    return 1;
}

/* the base of the test builds' own server (TNFS_UPDATE_API_BASE): letters, digits and : / . _ - only, so that nothing but a plain address reaches the shell */
static int base_is_plain(const char *base)
{
    if (base == NULL || base[0] == '\0' || strlen(base) > 120) return 0;
    for (const char *c = base; *c != '\0'; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr(":/._-", *c) != NULL)) return 0;
    return 1;
}

int update_api_command_at(char *out, size_t cap, const char *base, int allow_http, const char *event, const char *version)
{
    char v[API_VERSION_MAX + 1];
    if (cap == 0) return 0;
    out[0] = '\0';
    if (!base_is_plain(base) || !update_api_version(version, v, sizeof v)) return 0;
    const char *proto = allow_http ? "--proto =http,https --proto-redir =http,https" : "--proto =https --proto-redir =https";
    int n;
    if (event == NULL) {
        n = snprintf(out, cap, UPDATE_CURL " -fsSL %s --max-time 5 -A " UPDATE_API_NAME " "
                               "\"%s/latest?v=%s&p=" UPDATE_API_PLATFORM "\"" UPDATE_STDERR_TO_NULL, proto, base, v);
    } else {
        if (strcmp(event, "first_start") != 0 && strcmp(event, "data_ready") != 0 && strcmp(event, "first_race") != 0) return 0;
        n = snprintf(out, cap, UPDATE_CURL " -fsS -X POST %s --max-time 5 -A " UPDATE_API_NAME " "
                               "-o " UPDATE_NULL_DEV " \"%s/event?e=%s&v=%s&p=" UPDATE_API_PLATFORM "\"" UPDATE_STDERR_TO_NULL, proto, base, event, v);
    }
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

int update_windows_command(const char *command, const char *system_dir, char *out, size_t cap)
{
    static const char CURL[] = "curl.exe";
    if (cap == 0) return 0;
    out[0] = '\0';
    if (command == NULL || system_dir == NULL || system_dir[0] == '\0' || strchr(system_dir, '"') != NULL) return 0;
    if (strncmp(command, CURL, sizeof CURL - 1) != 0 || (command[sizeof CURL - 1] != ' ' && command[sizeof CURL - 1] != '\0')) return 0;
    const size_t dir = strlen(system_dir);
    const int slash = system_dir[dir - 1] == '\\' || system_dir[dir - 1] == '/';
    const int n = snprintf(out, cap, "\"%s%s%s\"%s", system_dir, slash ? "" : "\\", CURL, command + sizeof CURL - 1);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

void update_curl_reason(int status, char *out, size_t cap)
{
    static const struct {
        int status;
        const char *means;
    } known[] = {
        { 5, "the proxy's name was not resolved" }, { 6, "the server's name was not resolved: offline, or no DNS" }, { 7, "no connection to the server" },
        { 22, "an HTTP error from the server: refused or rate limited" }, { 28, "timed out" }, { 35, "the TLS handshake failed" }, { 60, "the server's certificate was not accepted" },
        { 126, UPDATE_CURL " could not be run" }, { 127, UPDATE_CURL " not found" },
    };
    if (status < 0) {
        snprintf(out, cap, "curl did not end normally");
        return;
    }
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
        if (known[i].status == status) {
            snprintf(out, cap, "curl failed, exit %d: %s", status, known[i].means);
            return;
        }
    snprintf(out, cap, "curl failed, exit %d (see curl's manual)", status);
}

int update_api_command(char *out, size_t cap, const char *event, const char *version)
{
    return update_api_command_at(out, cap, UPDATE_API_BASE, 0, event, version);
}
