#include "public_ip.h"
#include "output.h"

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

#define PUBLIC_IP_TIMEOUT_MS 1500

static char    g_ip[64] = {0};
static HANDLE  g_evt = NULL;
static HANDLE  g_thr = NULL;

static DWORD WINAPI fetch_proc(LPVOID arg)
{
    (void)arg;
    HINTERNET hSession = WinHttpOpen(L"hardfetch/0.2",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession) {
        WinHttpSetTimeouts(hSession,
            PUBLIC_IP_TIMEOUT_MS, PUBLIC_IP_TIMEOUT_MS,
            PUBLIC_IP_TIMEOUT_MS, PUBLIC_IP_TIMEOUT_MS);

        HINTERNET hConnect = WinHttpConnect(hSession, L"api.ipify.org",
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/",
                NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                    && WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD total = 0, avail = 0, read = 0;
                    while (total < sizeof(g_ip) - 1
                           && WinHttpQueryDataAvailable(hRequest, &avail)
                           && avail > 0) {
                        DWORD want = (avail < sizeof(g_ip) - 1 - total)
                                     ? avail : (DWORD)(sizeof(g_ip) - 1 - total);
                        if (!WinHttpReadData(hRequest, g_ip + total, want, &read)
                            || read == 0) break;
                        total += read;
                    }
                    g_ip[total] = '\0';
                    while (total > 0 && (g_ip[total-1] == '\n' || g_ip[total-1] == '\r'
                                         || g_ip[total-1] == ' ' || g_ip[total-1] == '\t'))
                        g_ip[--total] = '\0';
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    if (g_evt) SetEvent(g_evt);
    return 0;
}

void public_ip_start(void)
{
    if (g_thr) return;
    g_evt = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_thr = CreateThread(NULL, 0, fetch_proc, NULL, 0, NULL);
}

void public_ip_print(void)
{
    int ready = 0;
    if (g_evt && WaitForSingleObject(g_evt, 0) == WAIT_OBJECT_0)
        ready = 1;

    if (ready && g_ip[0])
        print_block("Public", g_ip);
    else
        print_block("Public", "N/A");

    if (g_thr) { CloseHandle(g_thr); g_thr = NULL; }
    if (g_evt) { CloseHandle(g_evt); g_evt = NULL; }
}
