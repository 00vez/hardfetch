#include "ip_addr.h"
#include "output.h"

#include <stdio.h>
#include <stdlib.h>
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
    if (ret != ERROR_SUCCESS) { free(buf); return; }

    for (IP_ADAPTER_ADDRESSES* a = buf; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if ((ULONG)a->IfType == IF_TYPE_LOOPBACK) continue;

        const wchar_t* wn = a->FriendlyName ? a->FriendlyName : a->Description;
        char name[11] = {0};
        if (wn)
            WideCharToMultiByte(CP_UTF8, 0, wn, -1, name, sizeof(name) - 1, NULL, NULL);
        if (!name[0])
            memcpy(name, "Net", 4);

        int shown = 0;
        for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            SOCKADDR_IN* sa = (SOCKADDR_IN*)u->Address.lpSockaddr;
            char ipStr[64];
            inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));

            char val[96];
            ULONG prefix = u->OnLinkPrefixLength;
            if (prefix) snprintf(val, sizeof(val), "%s/%lu", ipStr, prefix);
            else        snprintf(val, sizeof(val), "%s", ipStr);

            const char* lbl = shown ? "  " : name;
            if (shown)
                printf("            \x1b[90m%-10s\x1b[0m  \x1b[97m%s\x1b[0m\n", lbl, val);
            else
                print_block(lbl, val);
            shown++;
        }
    }
    free(buf);
}
