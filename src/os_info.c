#include "os_info.h"
#include "output.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static char s_product[256] = {0};
static char s_build[64] = {0};
static char s_display_version[64] = {0};
static char s_arch[16] = "x86_64";
static int s_initialized = 0;

static void init_os_info(void)
{
    if (s_initialized) return;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(s_product);
        RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)s_product, &size);
        size = sizeof(s_build);
        RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, NULL, (LPBYTE)s_build, &size);
        size = sizeof(s_display_version);
        if (RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL, (LPBYTE)s_display_version, &size) != ERROR_SUCCESS)
            s_display_version[0] = '\0';
        RegCloseKey(hKey);
    }

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: strcpy_s(s_arch, sizeof(s_arch), "x86_64"); break;
        case PROCESSOR_ARCHITECTURE_ARM64: strcpy_s(s_arch, sizeof(s_arch), "aarch64"); break;
        case PROCESSOR_ARCHITECTURE_INTEL: strcpy_s(s_arch, sizeof(s_arch), "x86"); break;
        default: s_arch[0] = '\0';
    }

    unsigned long buildNum = 0;
    if (s_build[0]) buildNum = strtoul(s_build, NULL, 10);
    if (buildNum >= 22000) {
        char* win10 = strstr(s_product, "Windows 10");
        if (win10) memcpy_s(win10, strlen(win10), "Windows 11", 10);
    }

    s_initialized = 1;
}

void print_os_info(void)
{
    init_os_info();
    if (s_product[0] && s_build[0]) {
        char buf[512];
        int pos = snprintf(buf, sizeof(buf), "%s %s", s_product, s_build);
        if (s_display_version[0])
            pos += snprintf(buf + pos, sizeof(buf) - pos, " (%s)", s_display_version);
        if (s_arch[0])
            pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  %s", s_arch);
        print_block("OS", buf);
    }
}

const char* get_os_product(void)
{
    init_os_info();
    return s_product;
}

const char* get_os_build(void)
{
    init_os_info();
    return s_build;
}

const char* get_os_display_version(void)
{
    init_os_info();
    return s_display_version;
}
