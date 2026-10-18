#define _POSIX_C_SOURCE 200809L
#include "debug.h"
#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int s_enabled = 0;

/* The module name and start time are per thread: main and the worker both
   log, and shared state would interleave and mislabel them. */
#if defined(_MSC_VER)
#define HF_THREAD_LOCAL __declspec(thread)
#else
#define HF_THREAD_LOCAL __thread
#endif

static HF_THREAD_LOCAL char s_module[32] = "?";
static HF_THREAD_LOCAL double s_start_ms = 0;
static double s_freq_hz = 0;

static double monotonic_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER c;
#else
    struct timespec ts;
#endif
    if (s_freq_hz == 0) {
#ifdef _WIN32
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq)) s_freq_hz = (double)freq.QuadPart;
#else
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
            s_freq_hz = 1000000000.0;
#endif
    }
#ifdef _WIN32
    if (!QueryPerformanceCounter(&c) || s_freq_hz == 0) return 0;
    return (double)c.QuadPart * 1000.0 / s_freq_hz;
#else
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

void debug_enable(void)
{
    s_enabled = 1;
}

int debug_is_enabled(void)
{
    return s_enabled;
}

void debug_begin(const char* module)
{
    if (!s_enabled) return;
    snprintf(s_module, sizeof(s_module), "%s", module ? module : "?");
    s_start_ms = monotonic_ms();
}

void debug_log(const char* fmt, ...)
{
    va_list ap;

    if (!s_enabled) return;
    fprintf(stderr, "[%-8s] ", s_module);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(OUTPUT_NL, stderr);
}

void debug_end(void)
{
    double ms;

    if (!s_enabled) return;
    ms = monotonic_ms() - s_start_ms;
    fprintf(stderr, "[%-8s] --- %.2f ms" OUTPUT_NL, s_module, ms);
}

void debug_raw(const char* label, const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*)data;
    size_t i, j;

    if (!s_enabled || !p) return;
    fprintf(stderr, "[%-8s] %s (%zu bytes):" OUTPUT_NL,
            s_module, label ? label : "raw", len);
    for (i = 0; i < len; i += 16) {
        fprintf(stderr, "    %04zx  ", i);
        for (j = 0; j < 16 && i + j < len; j++)
            fprintf(stderr, "%02x ", p[i + j]);
        for (; j < 16; j++)
            fputs("   ", stderr);
        fputc(' ', stderr);
        for (j = 0; j < 16 && i + j < len; j++)
            fputc(p[i + j] >= 32 && p[i + j] < 127 ? (int)p[i + j] : '.', stderr);
        fputs(OUTPUT_NL, stderr);
    }
}

#ifdef _WIN32

const char* debug_win32(unsigned long err)
{
    static char buf[256];
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, (DWORD)err, 0, buf, (DWORD)sizeof(buf) - 1, NULL);
    if (n == 0) {
        snprintf(buf, sizeof(buf), "Win32 error %lu", err);
        return buf;
    }
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return buf;
}

const char* debug_hresult(long hr)
{
    static char buf[128];
    const char* name = "unknown HRESULT";

    switch ((unsigned long)hr) {
        case 0x00000000UL: name = "S_OK"; break;
        case 0x00000001UL: name = "S_FALSE"; break;
        case 0x80004001UL: name = "E_NOTIMPL"; break;
        case 0x80070032UL: name = "E_NOTIMPL"; break;
        case 0x80070057UL: name = "E_INVALIDARG"; break;
        case 0x8007000EUL: name = "E_OUTOFMEMORY"; break;
        case 0x80070005UL: name = "E_ACCESSDENIED"; break;
        case 0x8001010AUL: name = "E_NOINTERFACE"; break;
        case 0x800706BAUL: name = "RPC_S_SERVER_UNAVAILABLE"; break;
        case 0x800707D8UL: name = "HRESULT_FROM_WIN32(ERROR_TIMEOUT)"; break;
        default: break;
    }
    snprintf(buf, sizeof(buf), "0x%08lX %s", (unsigned long)hr, name);
    return buf;
}

#endif /* _WIN32 */
