#include "../host.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void read_trim(const char* path, char* out, size_t n)
{
    out[0] = '\0';
    FILE* f = fopen(path, "r");
    if (!f) return;
    if (!fgets(out, (int)n, f)) { fclose(f); return; }
    fclose(f);
    size_t l = strlen(out);
    while (l > 0 && (out[l-1] == '\n' || out[l-1] == '\r' ||
                      out[l-1] == ' '  || out[l-1] == '\t'))
        out[--l] = '\0';
}

void print_host_info(void)
{
    char name[128] = {0};
    read_trim("/sys/class/dmi/id/product_name", name, sizeof(name));
    if (!name[0])
        read_trim("/sys/devices/virtual/dmi/id/product_name", name, sizeof(name));
    if (!name[0])
        read_trim("/sys/class/dmi/id/board_name", name, sizeof(name));
    if (!name[0]) {
        char host[256] = {0};
        if (gethostname(host, sizeof(host) - 1) == 0)
            snprintf(name, sizeof(name), "%s", host);
    }
    print_block("Host", name[0] ? name : "N/A");
}
