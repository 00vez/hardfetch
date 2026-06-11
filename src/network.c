#include "network.h"
#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#define CF_HOST L"speed.cloudflare.com"
#define PING_COUNT 15
#define DOWN_THREADS 6
#define DOWN_BYTES 26214400
#define UP_THREADS 4
#define UP_BYTES 2097152
#define TIMEOUT_MS 10000

typedef struct {
  double ping_ms;
  double jitter_ms;
  double down_mbps;
  double up_mbps;
  int ping_ok;
  int down_ok;
  int up_ok;
} NetResult;

static NetResult g_net = {0};
static HANDLE g_thread = NULL;

static double timer_sec(void)
{
  static LARGE_INTEGER freq = {0};
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / freq.QuadPart;
}

static double timer_ms(void) { return timer_sec() * 1000.0; }

static int cmp_double(const void* a, const void* b)
{
  double da = *(const double*)a, db = *(const double*)b;
  return (da > db) - (da < db);
}

static int http_latency(double* samples, int count)
{
  HINTERNET sess = WinHttpOpen(L"hardfetch/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               NULL, NULL, 0);
  if (!sess) return -1;

  int ok = 0;
  HINTERNET conn = WinHttpConnect(sess, CF_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!conn) { WinHttpCloseHandle(sess); return -1; }

  for (int i = 0; i < count; i++) {
    double t0 = timer_ms();
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", L"/__down?bytes=0",
                                        NULL, NULL, NULL,
                                        WINHTTP_FLAG_SECURE);
    if (!req) continue;

    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
      double dt = timer_ms() - t0;
      samples[ok++] = dt;
    }
    WinHttpCloseHandle(req);
  }

  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(sess);
  return ok;
}

typedef struct {
  int index;
  int is_upload;
  long long bytes;
  double elapsed_ms;
  int ok;
} WorkerResult;

static DWORD WINAPI down_worker(LPVOID param)
{
  WorkerResult* wr = (WorkerResult*)param;
  char url[128];
  snprintf(url, sizeof(url), "/__down?bytes=%d", DOWN_BYTES);

  HINTERNET sess = WinHttpOpen(L"hardfetch/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               NULL, NULL, 0);
  if (!sess) { wr->ok = 0; return 0; }

  HINTERNET conn = WinHttpConnect(sess, CF_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!conn) { WinHttpCloseHandle(sess); wr->ok = 0; return 0; }

  wchar_t wurl[512];
  MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 512);

  double t0 = timer_ms();
  HINTERNET req = WinHttpOpenRequest(conn, L"GET", wurl, NULL, NULL, NULL,
                                      WINHTTP_FLAG_SECURE);
  if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); wr->ok = 0; return 0; }

  DWORD timeout = TIMEOUT_MS;
  WinHttpSetOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

  int success = 0;
  if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(req, NULL)) {
    BYTE buf[65536];
    DWORD read = 0;
    long long total = 0;
    while (WinHttpReadData(req, buf, sizeof(buf), &read) && read > 0)
      total += read;
    if (total >= (long long)DOWN_BYTES * 0.9) success = 1;
  }

  WinHttpCloseHandle(req);
  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(sess);

  if (success) {
    wr->bytes = DOWN_BYTES;
    wr->elapsed_ms = timer_ms() - t0;
    wr->ok = 1;
  } else {
    wr->ok = 0;
  }
  return 0;
}

static DWORD WINAPI up_worker(LPVOID param)
{
  WorkerResult* wr = (WorkerResult*)param;
  BYTE* payload = (BYTE*)malloc(UP_BYTES);
  if (!payload) { wr->ok = 0; return 0; }
  for (int i = 0; i < UP_BYTES; i++)
    payload[i] = (BYTE)(rand() & 0xFF);

  HINTERNET sess = WinHttpOpen(L"hardfetch/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               NULL, NULL, 0);
  if (!sess) { free(payload); wr->ok = 0; return 0; }

  HINTERNET conn = WinHttpConnect(sess, CF_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!conn) { WinHttpCloseHandle(sess); free(payload); wr->ok = 0; return 0; }

  double t0 = timer_ms();
  HINTERNET req = WinHttpOpenRequest(conn, L"POST", L"/__up", NULL, NULL, NULL,
                                      WINHTTP_FLAG_SECURE);
  if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); free(payload); wr->ok = 0; return 0; }

  DWORD timeout = TIMEOUT_MS;
  WinHttpSetOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

  LPCWSTR headers = L"Content-Type: application/octet-stream\r\n";
  if (WinHttpSendRequest(req, headers, (DWORD)wcslen(headers),
                         NULL, 0, UP_BYTES, 0)) {
    DWORD written = 0;
    WinHttpWriteData(req, payload, UP_BYTES, &written);
  }

  int success = 0;
  if (WinHttpReceiveResponse(req, NULL))
    success = 1;

  WinHttpCloseHandle(req);
  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(sess);
  free(payload);

  if (success) {
    wr->bytes = UP_BYTES;
    wr->elapsed_ms = timer_ms() - t0;
    wr->ok = 1;
  } else {
    wr->ok = 0;
  }
  return 0;
}

static int run_parallel(int count, LPTHREAD_START_ROUTINE worker,
                        double* out_mbps)
{
  WorkerResult* results = (WorkerResult*)calloc(count, sizeof(WorkerResult));
  HANDLE* threads = (HANDLE*)calloc(count, sizeof(HANDLE));
  if (!results || !threads) { free(results); free(threads); return 0; }

  double t0 = timer_ms();
  for (int i = 0; i < count; i++) {
    results[i].index = i;
    threads[i] = CreateThread(NULL, 0, worker, &results[i], 0, NULL);
  }

  WaitForMultipleObjects(count, threads, TRUE, TIMEOUT_MS + 5000);
  double wall = timer_ms() - t0;

  for (int i = 0; i < count; i++) {
    if (threads[i]) CloseHandle(threads[i]);
  }

  long long total = 0;
  int ok = 0;
  for (int i = 0; i < count; i++) {
    if (results[i].ok) { total += results[i].bytes; ok++; }
  }

  double mbps = 0;
  if (ok > 0 && wall > 0)
    mbps = (total / (double)(1024 * 1024)) / (wall / 1000.0) * 8.0;

  free(results);
  free(threads);

  if (mbps > 0) {
    *out_mbps = mbps;
    return 1;
  }
  return 0;
}

static DWORD WINAPI net_worker(LPVOID param)
{
  (void)param;

  double samples[PING_COUNT];
  int n = http_latency(samples, PING_COUNT);
  if (n >= 3) {
    qsort(samples, n, sizeof(double), cmp_double);
    double median = (n % 2) ? samples[n / 2]
                            : (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    double sumSq = 0;
    for (int i = 0; i < n; i++) {
      double d = samples[i] - median;
      sumSq += d * d;
    }
    g_net.ping_ms = median;
    g_net.jitter_ms = sqrt(sumSq / n);
    g_net.ping_ok = 1;
  }

  g_net.down_ok = run_parallel(DOWN_THREADS, down_worker,
                               &g_net.down_mbps);
  g_net.up_ok = run_parallel(UP_THREADS, up_worker,
                             &g_net.up_mbps);
  return 0;
}

void net_start_async(void)
{
  memset(&g_net, 0, sizeof(g_net));
  srand((unsigned int)GetTickCount64());
  g_thread = CreateThread(NULL, 0, net_worker, NULL, 0, NULL);
}

void net_wait(void)
{
  if (g_thread) {
    WaitForSingleObject(g_thread, 60000);
    CloseHandle(g_thread);
    g_thread = NULL;
  }
}

void net_print(void)
{
  char buf[512];
  int pos = 0;

  if (g_net.ping_ok) {
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "Ping %.0f ms  |  Jitter %.0f ms", g_net.ping_ms, g_net.jitter_ms);
  } else {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "Ping N/A");
  }

  if (g_net.down_ok)
    pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  Down %.0f Mbps", g_net.down_mbps);

  if (g_net.up_ok)
    pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  Up %.0f Mbps", g_net.up_mbps);

  print_value(buf);
  print_newline();
}
