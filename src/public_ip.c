#include "public_ip.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

/* Same contract as the POSIX side: the lookup runs on its own thread so a
   slow endpoint or a wedged resolver can never hold up the first frame. */

typedef struct {
    char result[64];
    unsigned int timeoutMs;
    int done;
} lookup_state;

static lookup_state g_lk;
static HANDLE      g_thread = NULL;

static BOOL fetch_one(const wchar_t* host, const wchar_t* path, unsigned int timeoutMs,
                      char* out, size_t outLen)
{
    HINTERNET hSession = WinHttpOpen(L"hardfetch/3.3",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    BOOL exchanged = FALSE;
    size_t total = 0;

    out[0] = 0;
    if (!hSession) return FALSE;
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    {
        HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) goto done;
        {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) { WinHttpCloseHandle(hConnect); goto done; }

            /* Success of the exchange is separate from there being more body
               to read: running out of data is the normal end, not a failure. */
            exchanged = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                         WinHttpReceiveResponse(hRequest, NULL);

            if (exchanged) {
                DWORD avail = 0;
                while (total + 1 < outLen &&
                       WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                    DWORD got = 0;
                    if (!WinHttpReadData(hRequest, out + total,
                                         (DWORD)(outLen - 1 - total), &got) || got == 0)
                        break;
                    total += got;
                }
            }

            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }

done:
    WinHttpCloseHandle(hSession);
    if (!exchanged || total == 0) { out[0] = 0; return FALSE; }
    out[total] = 0;

    while (total > 0 && (out[total - 1] == '\n' || out[total - 1] == '\r' ||
                         out[total - 1] == ' ' || out[total - 1] == '\t'))
        out[--total] = 0;

    {
        int dots = 0, digits = 0;
        size_t i;
        for (i = 0; i < total; i++) {
            if (out[i] == '.') dots++;
            else if (out[i] >= '0' && out[i] <= '9') digits++;
            else return FALSE;
        }
        if (dots != 3 || digits < 7) { out[0] = 0; return FALSE; }
    }
    return TRUE;
}

static DWORD WINAPI thread_proc(LPVOID arg)
{
    lookup_state* lk = (lookup_state*)arg;
    static const wchar_t* hosts[] = { L"api.ipify.org", L"ipv4.icanhazip.com" };
    static const wchar_t* paths[] = { L"/", L"/" };
    size_t i;

    lk->result[0] = 0;
    for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
        if (fetch_one(hosts[i], paths[i], lk->timeoutMs, lk->result, sizeof(lk->result)))
            break;
        lk->result[0] = 0;
    }
    lk->done = 1;
    return 0;
}

void public_ip_start(hf_state* s, unsigned int timeoutMs)
{
    (void)s;

    debug_begin("publicip");
    if (g_thread) { debug_end(); return; }

    ZeroMemory(&g_lk, sizeof(g_lk));
    g_lk.timeoutMs = timeoutMs;

    g_thread = CreateThread(NULL, 0, thread_proc, &g_lk, 0, NULL);
    if (!g_thread)
        debug_log("could not start the lookup thread");
    else
        debug_log("lookup thread started");
    debug_end();
}

int public_ip_join(hf_state* s, unsigned int waitMs)
{
    DWORD rc;

    debug_begin("publicip");
    if (!g_thread) { debug_end(); return 0; }

    rc = WaitForSingleObject(g_thread, waitMs);
    if (rc != WAIT_OBJECT_0 || !g_lk.done || !g_lk.result[0]) {
        debug_log("no public IP within %u ms", waitMs);
        s->ip_public[0] = 0;
        CloseHandle(g_thread);
        g_thread = NULL;
        debug_end();
        return 0;
    }

    snprintf(s->ip_public, sizeof(s->ip_public), "%s", g_lk.result);
    debug_log("public IP = %s", s->ip_public);
    CloseHandle(g_thread);
    g_thread = NULL;
    debug_end();
    return 1;
}
