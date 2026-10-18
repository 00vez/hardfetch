#define _GNU_SOURCE
#include "../ip_addr.h"
#include "../debug.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif

#define MAX_IFACES 16

typedef struct {
    char label[24];
    char value[96];
    int  score;
} iface;

static unsigned int prefix_from_netmask(const struct sockaddr_in* nm)
{
    const unsigned char* b = (const unsigned char*)&nm->sin_addr;
    unsigned int prefix = 0;
    int i;

    for (i = 0; i < 4; i++) {
        unsigned char v = b[i];
        while (v) { prefix += (unsigned int)(v & 1); v >>= 1; }
    }
    return prefix;
}

/* Wireless first, then ethernet, matching the Windows scoring so both
   platforms pick the same adapter when several are up. */
static int ifname_score(const char* name)
{
    if (!name) return 0;
    if (strncmp(name, "wl", 2) == 0) return 100;
    if (strncmp(name, "en", 2) == 0 || strncmp(name, "eth", 3) == 0) return 90;
    if (strncmp(name, "ppp", 3) == 0 || strncmp(name, "wg", 2) == 0) return 60;
    if (strncmp(name, "tailscale", 9) == 0 || strncmp(name, "ts", 2) == 0) return 40;
    if (strncmp(name, "docker", 6) == 0 || strncmp(name, "veth", 4) == 0) return 20;
    if (strncmp(name, "lo", 2) == 0) return 5;
    return 10;
}

/* The interface carrying the default route is the one whose address a
   request would actually leave from, so it outranks any name based guess.
   Windows reaches the same conclusion through its interface scoring. */
static int default_route_iface(char* out, size_t outLen)
{
    FILE* f = fopen("/proc/net/route", "r");
    char line[256];
    int found = 0;

    out[0] = 0;
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }   /* header */

    while (fgets(line, sizeof(line), f)) {
        char ifname[32];
        char dest[64];
        char gw[64];

        if (sscanf(line, "%31s %63s %63s", ifname, dest, gw) != 3) continue;
        /* Destination 00000000 with a non-zero gateway is the default route. */
        if (strcmp(dest, "00000000") != 0) continue;
        if (strcmp(gw, "00000000") == 0) continue;
        snprintf(out, outLen, "%s", ifname);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void swap_iface(iface* a, iface* b)
{
    iface t = *a;
    *a = *b;
    *b = t;
}

void ip_collect(hf_state* s, int show_all)
{
    struct ifaddrs* ifa = NULL;
    struct ifaddrs* a;
    iface list[MAX_IFACES];
    char route[32] = {0};
    int count = 0;

    debug_begin("ip");

    s->ip_local[0] = 0;
    s->ip_label[0] = 0;
    s->ip_iface_count = 0;

    if (getifaddrs(&ifa) != 0 || !ifa) {
        debug_log("getifaddrs failed");
        debug_end();
        return;
    }

    if (!default_route_iface(route, sizeof(route)))
        debug_log("no default route found, scoring by interface name only");

    for (a = ifa; a && count < MAX_IFACES; a = a->ifa_next) {
        struct sockaddr_in* sa;
        char ip[64];
        iface e;

        if (!a->ifa_addr) continue;
        if (a->ifa_addr->sa_family != AF_INET) continue;
        if (!(a->ifa_flags & IFF_UP)) continue;
        if (a->ifa_flags & IFF_LOOPBACK) continue;

        sa = (struct sockaddr_in*)a->ifa_addr;
        if (!inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) continue;

        memset(&e, 0, sizeof(e));
        snprintf(e.label, sizeof(e.label), "%s", a->ifa_name ? a->ifa_name : "Net");
        if (a->ifa_netmask) {
            unsigned int pfx = prefix_from_netmask(
                (const struct sockaddr_in*)a->ifa_netmask);
            snprintf(e.value, sizeof(e.value), "%s/%u", ip, pfx);
        } else {
            snprintf(e.value, sizeof(e.value), "%s", ip);
        }
        e.score = ifname_score(a->ifa_name);
        if (route[0] && strcmp(route, a->ifa_name) == 0)
            e.score += 1000;
        list[count++] = e;
    }
    freeifaddrs(ifa);

    if (count == 0) {
        debug_log("no operational IPv4 interface");
        debug_end();
        return;
    }

    /* Best first. */
    for (int i = 1; i < count; i++)
        if (list[i].score > list[0].score) swap_iface(&list[0], &list[i]);

    snprintf(s->ip_label, sizeof(s->ip_label), "%s", list[0].label);
    snprintf(s->ip_local, sizeof(s->ip_local), "%s", list[0].value);
    debug_log("best interface '%s' -> %s (score %d)",
              list[0].label, list[0].value, list[0].score);

    /* The rest alphabetically, like the Windows build. */
    for (int i = 1; i < count; i++)
        for (int j = i + 1; j < count; j++)
            if (strcmp(list[i].label, list[j].label) > 0)
                swap_iface(&list[i], &list[j]);

    for (int i = 1; i < count && s->ip_iface_count < 8; i++) {
        snprintf(s->ip_ifaces[s->ip_iface_count].label, 24, "%s", list[i].label);
        snprintf(s->ip_ifaces[s->ip_iface_count].value, 48, "%s", list[i].value);
        s->ip_iface_count++;
    }

    debug_log("%d interface(s) total", count);
    debug_end();
}
