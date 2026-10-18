#include "ip_addr.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#ifndef IF_TYPE_LOOPBACK
#define IF_TYPE_LOOPBACK 24
#endif

typedef struct {
    char label[24];
    char value[96];
    int  score;
    int  index;
} Iface;

/* Highest preference first. */
static int iftype_score(ULONG t)
{
    switch (t) {
        case 71:  return 100;   /* IF_TYPE_IEEE80211, preferred like the sample output */
        case 6:   return 90;    /* IF_TYPE_ETHERNET_CSMACD */
        case 237: return 85;    /* IF_TYPE_IEEE8023AD_LAN */
        case 23:  return 60;    /* IF_TYPE_PPP */
        case 131: return 40;    /* IF_TYPE_TUNNEL */
        default:  return 10;
    }
}

static void shorten_label(const wchar_t* friendly, const wchar_t* description,
                          char* out, size_t outLen)
{
    const wchar_t* src = (friendly && friendly[0]) ? friendly : description;
    if (!src || WideCharToMultiByte(CP_UTF8, 0, src, -1, out, (int)outLen, NULL, NULL) == 0)
        out[0] = 0;
    if (!out[0]) snprintf(out, outLen, "Net");
}

static int compare_iface(const void* a, const void* b)
{
    return _stricmp(((const Iface*)a)->label, ((const Iface*)b)->label);
}

static int collect_ipv4(IP_ADAPTER_ADDRESSES* buf, Iface* out, int max)
{
    int count = 0;

    for (IP_ADAPTER_ADDRESSES* a = buf; a && count < max; a = a->Next) {
        IP_ADAPTER_UNICAST_ADDRESS* u;
        Iface cur;
        int have = 0;

        if (a->OperStatus != IfOperStatusUp) continue;
        if ((ULONG)a->IfType == IF_TYPE_LOOPBACK) continue;

        ZeroMemory(&cur, sizeof(cur));
        cur.index = (int)a->IfIndex;
        cur.score = iftype_score((ULONG)a->IfType);
        shorten_label(a->FriendlyName, a->Description, cur.label, sizeof(cur.label));

        for (u = a->FirstUnicastAddress; u; u = u->Next) {
            SOCKADDR_IN* sa;
            char ip[64];
            ULONG prefix;

            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            sa = (SOCKADDR_IN*)u->Address.lpSockaddr;
            if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip)) == NULL) continue;

            prefix = u->OnLinkPrefixLength;
            if (prefix)
                snprintf(cur.value, sizeof(cur.value), "%s/%lu", ip, (unsigned long)prefix);
            else
                snprintf(cur.value, sizeof(cur.value), "%s", ip);
            have = 1;
            break;
        }
        if (!have) continue;

        if (count == 0 || cur.score > out[0].score) {
            if (count > 0) out[count] = out[0];
            if (count + 1 < max) count++;
            out[0] = cur;
        } else if (count < max) {
            out[count++] = cur;
        }
    }
    return count;
}

void ip_collect(hf_state* s, int show_all)
{
    IP_ADAPTER_ADDRESSES* buf = NULL;
    ULONG bufLen = 0;
    DWORD ret;
    Iface ifaces[16];
    int count = 0;

    debug_begin("ip");

    s->ip_local[0] = 0;
    s->ip_label[0] = 0;
    s->ip_iface_count = 0;
    ZeroMemory(ifaces, sizeof(ifaces));

    for (int attempt = 0; attempt < 4; attempt++) {
        ret = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            NULL, buf, &bufLen);
        if (ret == ERROR_SUCCESS) break;
        if (ret != ERROR_BUFFER_OVERFLOW) {
            debug_log("GetAdaptersAddresses failed: %s", debug_win32(ret));
            free(buf);
            return;
        }
        free(buf);
        buf = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
        if (!buf) return;
    }
    if (ret != ERROR_SUCCESS) {
        debug_log("GetAdaptersAddresses still failing after 4 attempts");
        free(buf);
        return;
    }

    count = collect_ipv4(buf, ifaces, 16);
    free(buf);

    /* The remaining adapters are listed alphabetically; entry 0 stays the
       best-scoring one because it is the primary interface. */
    if (count > 2)
        qsort(ifaces + 1, (size_t)(count - 1), sizeof(ifaces[0]), compare_iface);

    if (count == 0) {
        debug_log("no operational IPv4 adapter with an address");
        debug_end();
        return;
    }

    snprintf(s->ip_label, sizeof(s->ip_label), "%s", ifaces[0].label);
    snprintf(s->ip_local, sizeof(s->ip_local), "%s", ifaces[0].value);
    debug_log("best interface '%s' -> %s (score %d)",
              ifaces[0].label, ifaces[0].value, ifaces[0].score);

    if (show_all) {
        for (int i = 1; i < count && s->ip_iface_count < 8; i++) {
            snprintf(s->ip_ifaces[s->ip_iface_count].label,
                     sizeof(s->ip_ifaces[0].label), "%s", ifaces[i].label);
            snprintf(s->ip_ifaces[s->ip_iface_count].value,
                     sizeof(s->ip_ifaces[0].value), "%s", ifaces[i].value);
            s->ip_iface_count++;
        }
        debug_log("%d further interfaces collected", s->ip_iface_count);
    }

    debug_end();
}