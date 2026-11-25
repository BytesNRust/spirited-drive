/* port: see os_info.h. */
#include "game/platform/os_info.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#elif !defined(__EMSCRIPTEN__)
#include <sys/utsname.h>
#endif

const char *os_info_build_arch(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unknown";
#endif
}

void os_info_version(char *out, size_t cap)
{
    if (cap == 0) return;
    snprintf(out, cap, "unknown");
#if defined(_WIN32)
    /* RtlGetVersion tells the truth on every Windows (GetVersionEx tells what the manifest allows) */
    typedef LONG(WINAPI * GetVersionFn)(OSVERSIONINFOW *);
    const HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    const FARPROC found = nt != NULL ? GetProcAddress(nt, "RtlGetVersion") : NULL;
    GetVersionFn get = NULL;
    if (found != NULL) memcpy(&get, &found, sizeof get);
    OSVERSIONINFOW info;
    memset(&info, 0, sizeof info);
    info.dwOSVersionInfoSize = sizeof info;
    if (get != NULL && get(&info) == 0) snprintf(out, cap, "%lu.%lu (build %lu)", (unsigned long)info.dwMajorVersion, (unsigned long)info.dwMinorVersion, (unsigned long)info.dwBuildNumber);
#elif defined(__APPLE__)
    char version[64];
    size_t size = sizeof version;
    if (sysctlbyname("kern.osproductversion", version, &size, NULL, 0) == 0) snprintf(out, cap, "%s", version);
#elif !defined(__EMSCRIPTEN__)
    struct utsname name;
    if (uname(&name) == 0) snprintf(out, cap, "%s %s", name.sysname, name.release);
#endif
}
