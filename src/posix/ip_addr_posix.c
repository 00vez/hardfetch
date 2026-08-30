#define _GNU_SOURCE
#include "../ip_addr.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void print_ip_info(void)
{
    struct ifaddrs* ifa = NULL;
    if (getifaddrs(&ifa) != 0 || !ifa) { print_block("Net", "N/A"); return; }

    for (struct ifaddrs* a = ifa; a; a = a->ifa_next) {
        if (!a->ifa_addr) continue;
        if (a->ifa_addr->sa_family != AF_INET) continue;
        if (!(a->ifa_flags & IFF_UP)) continue;
        if (a->ifa_flags & IFF_LOOPBACK) continue;

        char name[11] = {0};
        if (a->ifa_name) {
            strncpy(name, a->ifa_name, sizeof(name) - 1);
        }
        if (!name[0]) strncpy(name, "Net", sizeof(name) - 1);

        char ipStr[64] = {0};
        inet_ntop(AF_INET,
                  &((struct sockaddr_in*)a->ifa_addr)->sin_addr,
                  ipStr, sizeof(ipStr));

        char val[96];
        if (a->ifa_netmask) {
            unsigned int prefix = 0;
            unsigned char* m = (unsigned char*)&((struct sockaddr_in*)a->ifa_netmask)->sin_addr;
            for (int i = 0; i < 4; i++) {
                unsigned char b = m[i];
                while (b) { prefix += (b & 1); b >>= 1; }
            }
            snprintf(val, sizeof(val), "%s/%u", ipStr, prefix);
        } else {
            snprintf(val, sizeof(val), "%s", ipStr);
        }
        print_block(name, val);
    }
    freeifaddrs(ifa);
}
