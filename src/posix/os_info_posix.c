#include "../os_info.h"
#include "../output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#if defined(__APPLE__)
static void mac_sw_vers(char* product, size_t product_sz, char* build, size_t build_sz)
{
    /* Apple wants version in product + build in release */
    struct utsname u;
    if (uname(&u) == 0) {
        strncpy(build, u.release, build_sz - 1);
        build[build_sz - 1] = '\0';
    }
    /* sw_vers -productVersion gives e.g. 14.5 */
    FILE* p = popen("/usr/bin/sw_vers -productVersion", "r");
    if (p) {
        char line[64] = {0};
        if (fgets(line, sizeof(line), p)) {
            char* nl = strchr(line, '\n'); if (nl) *nl = '\0';
            snprintf(product, product_sz, "macOS %s", line);
        }
        pclose(p);
    } else {
        snprintf(product, product_sz, "macOS");
    }
}
#endif

static char s_product[128];
static char s_build[128];
static char s_display[64];
static int s_init = 0;

static void init_once(void)
{
    if (s_init) return;
    s_product[0] = '\0'; s_build[0] = '\0';

#if defined(__APPLE__)
    mac_sw_vers(s_product, sizeof(s_product), s_build, sizeof(s_build));
#else
    struct utsname u;
    if (uname(&u) == 0)
        strncpy(s_build, u.release, sizeof(s_build) - 1);
    FILE* f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char* p = line + 12;
                if (*p == '"') { p++; char* q = strchr(p, '"'); if (q) *q = '\0'; }
                else { char* q = strchr(p, '\n'); if (q) *q = '\0'; }
                strncpy(s_product, p, sizeof(s_product) - 1);
                break;
            }
        }
        fclose(f);
    }
    if (getenv("WSL_DISTRO_NAME") || getenv("WSLENV")) {
        const char* tag = " (WSL2)";
        size_t cur = strlen(s_product);
        if (cur + strlen(tag) < sizeof(s_product)) strcat(s_product, tag);
    }
#endif
    s_init = 1;
}

void print_os_info(void)
{
    init_once();
    struct utsname u;
    const char* arch = "";
    if (uname(&u) == 0) arch = u.machine;

    char buf[512];
    const char* prod = s_product[0] ? s_product : u.sysname;
    const char* rel  = s_build[0] ? s_build : u.release;
    if (arch[0])
        snprintf(buf, sizeof(buf), "%s %s  |  %s", prod, rel, arch);
    else
        snprintf(buf, sizeof(buf), "%s %s", prod, rel);
    print_block("OS", buf);
}

const char* get_os_product(void)        { init_once(); return s_product[0] ? s_product : "macOS"; }
const char* get_os_build(void)          { init_once(); return s_build; }
const char* get_os_display_version(void){ init_once(); return s_display; }
