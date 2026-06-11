#include "ip_addr.h"
#include "output.h"

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#ifndef IF_TYPE_LOOPBACK
#define IF_TYPE_LOOPBACK 24
#endif

void print_ip_info(void)
{
    IP_ADAPTER_ADDRESSES* buf = NULL;
    ULONG bufLen = 0;
    DWORD ret;

    for (int i = 0; i < 4; i++) {
        ret = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            NULL, buf, &bufLen);
        if (ret == ERROR_SUCCESS)
            break;
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(buf);
            buf = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
            if (!buf) return;
        } else {
            free(buf);
            return;
        }
    }

    if (ret != ERROR_SUCCESS) {
        free(buf);
        return;
    }

    IP_ADAPTER_ADDRESSES* best = NULL;
    int best_priority = 999;

    for (IP_ADAPTER_ADDRESSES* a = buf; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if ((ULONG)a->IfType == IF_TYPE_LOOPBACK) continue;

        ULONG type = (ULONG)a->IfType;

        int prio;
        if (type == IF_TYPE_IEEE80211)            prio = 0;
        else if (type == IF_TYPE_ETHERNET_CSMACD) prio = 1;
        else if (type == 131)                    prio = 3;
        else                                      prio = 2;

        if (prio >= best_priority) continue;
        best_priority = prio;
        best = a;
    }

    if (best) {
        for (IP_ADAPTER_UNICAST_ADDRESS* u = best->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;

            SOCKADDR_IN* sa = (SOCKADDR_IN*)u->Address.lpSockaddr;
            char ipStr[64];
            inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));

            ULONG prefixLen = u->OnLinkPrefixLength ? u->OnLinkPrefixLength : 0;

            char buf2[256];
            snprintf(buf2, sizeof(buf2), "%s/%lu", ipStr, prefixLen);
            print_block("IP", buf2);
            free(buf);
            return;
        }
    }

    free(buf);
}
