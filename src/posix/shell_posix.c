#define _POSIX_C_SOURCE 200809L
#include "../shell.h"
#include "../output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void print_shell_info(void)
{
    const char* sh = getenv("SHELL");
    if (!sh || !*sh) { print_block("Shell", "N/A"); return; }

    const char* base = strrchr(sh, '/');
    base = base ? base + 1 : sh;

    char ver[64] = {0};
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", sh);
    FILE* p = popen(cmd, "r");
    if (p) {
        char line[256] = {0};
        if (fgets(line, sizeof(line), p)) {
            char* nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            const char* v = strstr(line, "version ");
            if (v) {
                v += 8;
                while (*v == ' ') v++;
            } else {
                v = strchr(line, ' ');
                if (v) { v++; while (*v == ' ') v++; if (!isdigit((unsigned char)*v)) v = NULL; }
            }
            if (v) {
                const char* e = v;
                while (*e && !isspace((unsigned char)*e)) e++;
                size_t len = (size_t)(e - v);
                if (len > 0 && len < sizeof(ver)) {
                    memcpy(ver, v, len);
                    ver[len] = '\0';
                }
            }
        }
        pclose(p);
    }

    char buf[160];
    if (ver[0]) snprintf(buf, sizeof(buf), "%s %s", base, ver);
    else        snprintf(buf, sizeof(buf), "%s", base);
    print_block("Shell", buf);
}
