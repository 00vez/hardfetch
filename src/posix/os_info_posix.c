#include "../os_info.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

static void strip_quotes(char* s)
{
    size_t n = strlen(s);
    char* end;

    if (n >= 2 && s[0] == '"') {
        memmove(s, s + 1, n);
        n--;
        end = s + n;
        while (end > s && end[-1] != '"') end--;
        if (end > s) end[-1] = 0;
    }
    {
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) s[--len] = 0;
    }
}

static void os_release_value(const char* key, char* out, size_t outLen)
{
    FILE* f = fopen("/etc/os-release", "r");

    out[0] = 0;
    if (!f) return;
    while (fgets(out, (int)outLen, f)) {
        size_t klen = strlen(key);
        if (strncmp(out, key, klen) == 0 && out[klen] == '=') {
            memmove(out, out + klen + 1, strlen(out + klen + 1) + 1);
            strip_quotes(out);
            fclose(f);
            return;
        }
    }
    out[0] = 0;
    fclose(f);
}

#if defined(__APPLE__)
static void mac_sw_vers(char* product, size_t productSz, char* release, size_t releaseSz)
{
    struct utsname u;
    FILE* p;

    release[0] = 0;
    if (uname(&u) == 0)
        snprintf(release, releaseSz, "%s", u.release);

    p = popen("/usr/bin/sw_vers -productVersion", "r");
    if (p) {
        char line[64] = {0};
        if (fgets(line, sizeof(line), p)) {
            char* nl = strchr(line, '\n');
            if (nl) *nl = 0;
            snprintf(product, productSz, "macOS %s", line);
        }
        pclose(p);
    } else {
        snprintf(product, productSz, "macOS");
    }
}
#endif

void os_collect(hf_state* s)
{
    struct utsname u;
    int haveUname;

    debug_begin("os");
    haveUname = (uname(&u) == 0);

#if defined(__APPLE__)
    {
        char release[64] = {0};
        char* dot;

        mac_sw_vers(s->os_product, sizeof(s->os_product), release, sizeof(release));
        /* Darwin reports 23.5.0 while the marketing version is 14.5, so the
           first component is the version and the rest is the build, the same
           split the Windows build makes. */
        dot = strchr(release, '.');
        if (dot) {
            *dot = 0;
            snprintf(s->os_display_version, sizeof(s->os_display_version), "%s", release);
            snprintf(s->os_build_str, sizeof(s->os_build_str), "%s", dot + 1);
        } else {
            snprintf(s->os_display_version, sizeof(s->os_display_version), "%s", release);
            s->os_build_str[0] = 0;
        }
    }
#else
    /* NAME and VERSION_ID give the same shape as the Windows build, which
       reports "Windows 11 Pro 25H2": product and version as separate fields.
       PRETTY_NAME would repeat the version inside the product. */
    os_release_value("NAME", s->os_product, sizeof(s->os_product));
    os_release_value("VERSION_ID", s->os_display_version, sizeof(s->os_display_version));
    snprintf(s->os_build_str, sizeof(s->os_build_str), "%.*s",
             (int)sizeof(s->os_build_str) - 1, haveUname ? u.release : "");

    if (s->os_product[0] == 0 && haveUname)
        snprintf(s->os_product, sizeof(s->os_product), "%s", u.sysname);
#endif

    snprintf(s->os_arch, sizeof(s->os_arch), "%s", haveUname ? u.machine : "");

    debug_log("product='%s' version='%s' build='%s' arch='%s'",
              s->os_product, s->os_display_version, s->os_build_str, s->os_arch);
    debug_end();
}
