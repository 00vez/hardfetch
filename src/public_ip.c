#include "public_ip.h"
#include "debug.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

/* Tried in order; the first usable answer wins. */
const wchar_t* kHosts[] = { L"api.ipify.org", L"ipv4.icanhazip.com", L"ifconfig.me/ip" };
const wchar_t* kPaths[] = { L"/", L"/", L"/" };

static bool fetch_one(const wchar_t* host, const wchar_t* path, unsigned int timeoutMs,
               char* out, size_t outLen)
{
    HINTERNET hSession = WinHttpOpen(L"hardfetch/0.3",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    bool ok = false;
    size_t total = 0;

    if (!hSession) return false;
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    {
        HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) goto done;
        {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) { WinHttpCloseHandle(hConnect); goto done; }
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD avail = 0;
                ok = WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0;
                while (ok && total + 1 < outLen && avail > 0) {
                    DWORD want = (avail < outLen - 1 - total) ? avail : (DWORD)(outLen - 1 - total);
                    DWORD got = 0;
                    if (!WinHttpReadData(hRequest, out + total, want, &got) || got == 0) { ok = false; break; }
                    total += got;
                    if (!WinHttpQueryDataAvailable(hRequest, &avail)) { ok = false; break; }
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }

done:
    WinHttpCloseHandle(hSession);
    if (!ok || total == 0) { out[0] = '\0'; return false; }
    out[total] = '\0';
    while (total > 0) {
        char c = out[total - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') out[--total] = '\0';
        else break;
    }
    /* Accept only something that looks like an IPv4 literal. */
    {
        int dots = 0, digits = 0;
        for (size_t i = 0; i < total; i++) {
            if (out[i] == '.') dots++;
            else if (out[i] >= '0' && out[i] <= '9') digits++;
            else return false;
        }
        if (dots != 3 || digits < 7) return false;
    }
    return true;
}

void public_ip_fetch(hf_state* s, unsigned int timeoutMs)
{
    size_t i;

    debug_begin("publicip");

    s->ip_public[0] = '\0';

    for (i = 0; i < sizeof(kHosts) / sizeof(kHosts[0]); i++) {
        if (fetch_one(kHosts[i], kPaths[i], timeoutMs, s->ip_public, sizeof(s->ip_public))) {
            debug_log("public IP from '%ls' = %s", kHosts[i], s->ip_public);
            break;
        }
        debug_log("endpoint '%ls' failed or returned garbage", kHosts[i]);
        s->ip_public[0] = '\0';
    }

    if (!s->ip_public[0])
        debug_log("no public IP resolved");

    debug_end();
}