#include "../host.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

static void read_trim(const char* path, char* out, size_t n)
{
    FILE* f;
    size_t l;

    out[0] = 0;
    f = fopen(path, "r");
    if (!f) return;
    if (!fgets(out, (int)n, f)) { fclose(f); out[0] = 0; return; }
    fclose(f);
    l = strlen(out);
    while (l > 0 && (out[l - 1] == '\n' || out[l - 1] == '\r' ||
                     out[l - 1] == ' '  || out[l - 1] == '\t'))
        out[--l] = 0;
}

/* The header line is "hardfetch vX | user@host", so the user and the host
   have to be filled in before rendering. */
void ident_collect(hf_state* s)
{
    char buf[256] = {0};

    debug_begin("ident");

    snprintf(s->user_name, sizeof(s->user_name), "%s", "user");
    {
        const char* e = getenv("USER");
        if (!e || !*e) e = getenv("LOGNAME");
        if (e && *e) snprintf(s->user_name, sizeof(s->user_name), "%s", e);
    }
    if (gethostname(buf, sizeof(buf) - 1) != 0)
        buf[0] = 0;
    else {
        char* dot = strchr(buf, '.');
        if (dot) *dot = 0;
    }
    if (!buf[0])
        read_trim("/proc/sys/kernel/hostname", buf, sizeof(buf));
    snprintf(s->host_name, sizeof(s->host_name), "%s", buf[0] ? buf : "unknown");

    debug_log("user='%s' host='%s'", s->user_name, s->host_name);
    debug_end();
}

void host_collect(hf_state* s)
{
    debug_begin("host");

    read_trim("/sys/class/dmi/id/product_name", s->host, sizeof(s->host));
    if (!s->host[0])
        read_trim("/sys/devices/virtual/dmi/id/product_name", s->host, sizeof(s->host));
    if (!s->host[0])
        read_trim("/sys/class/dmi/id/board_name", s->host, sizeof(s->host));
    if (!s->host[0])
        snprintf(s->host, sizeof(s->host), "%s", s->host_name);

    debug_log("product_name='%s'", s->host);
    debug_end();
}
