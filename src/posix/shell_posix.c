#define _POSIX_C_SOURCE 200809L
#include "../shell.h"
#include "../debug.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shell_collect(hf_state* s)
{
    const char* sh;
    const char* base;
    char cmd[512];
    char line[256];
    FILE* p;

    debug_begin("shell");

    s->shell_name[0] = 0;
    s->shell_version[0] = 0;

    /* Interactive shells export SHELL; a login session may only have it in
       the environment of the terminal, so fall back to the parent name. */
    sh = getenv("SHELL");
    if (!sh || !*sh) sh = getenv("TERM_SHELL");
    if (!sh || !*sh) {
        debug_log("no SHELL in the environment");
        debug_end();
        return;
    }

    base = strrchr(sh, '/');
    base = base ? base + 1 : sh;
    snprintf(s->shell_name, sizeof(s->shell_name), "%s", base);

    snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", sh);
    p = popen(cmd, "r");
    if (p) {
        line[0] = 0;
        if (fgets(line, sizeof(line), p)) {
            char* nl = strchr(line, '\n');
            const char* v;
            char* e;

            if (nl) *nl = 0;
            v = strstr(line, "version ");
            if (v) {
                v += 8;
                while (*v == ' ') v++;
            } else {
                v = strchr(line, ' ');
                if (v) {
                    v++;
                    while (*v == ' ') v++;
                    if (!isdigit((unsigned char)*v)) v = NULL;
                }
            }
            if (v) {
                e = (char*)v;
                while (*e && !isspace((unsigned char)*e)) e++;
                if (e > v && (size_t)(e - v) < sizeof(s->shell_version))
                    snprintf(s->shell_version, sizeof(s->shell_version), "%.*s",
                             (int)(e - v), v);
            }
        }
        pclose(p);
    }

    debug_log("shell = '%s' '%s' from '%s'", s->shell_name, s->shell_version, sh);
    debug_end();
}
