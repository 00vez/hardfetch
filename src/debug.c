#include "debug.h"
#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

static int s_enabled = 0;
static LARGE_INTEGER s_freq;
static int s_have_freq = 0;

/* main thread and the worker thread both log, so the per-section state
   has to be thread local or the two interleave and mislabel each other. */
static __declspec(thread) char s_module[32] = "?";
static __declspec(thread) LARGE_INTEGER s_start;

void debug_enable(void)
{
    if (s_enabled) return;
    s_enabled = 1;
    if (!s_have_freq) {
        s_have_freq = QueryPerformanceFrequency(&s_freq) ? 1 : 0;
    }
}

int debug_is_enabled(void)
{
    return s_enabled;
}

void debug_begin(const char* module)
{
    if (!s_enabled) return;
    snprintf(s_module, sizeof(s_module), "%s", module ? module : "?");
    QueryPerformanceCounter(&s_start);
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
    LARGE_INTEGER now;
    double ms = 0.0;
    if (!s_enabled) return;
    QueryPerformanceCounter(&now);
    if (s_have_freq && s_freq.QuadPart > 0)
        ms = (double)(now.QuadPart - s_start.QuadPart) * 1000.0 / (double)s_freq.QuadPart;
    fprintf(stderr, "[%-8s] --- %.2f ms" OUTPUT_NL, s_module, ms);
}

void debug_raw(const char* label, const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*)data;
    size_t i;
    if (!s_enabled || !p) return;
    fprintf(stderr, "[%-8s] %s (%zu bytes):" OUTPUT_NL, s_module, label ? label : "raw", len);
    for (i = 0; i < len; i += 16) {
        size_t j;
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

static const char* hresult_str(unsigned long hr)
{
    switch (hr) {
        case 0x00000000UL: return "S_OK";
        case 0x00000001UL: return "S_FALSE";
        case 0x80004001UL: return "E_NOTIMPL";
        case 0x80070057UL: return "E_INVALIDARG";
        case 0x80070032UL: return "E_NOTIMPL";
        case 0x8007000EUL: return "E_OUTOFMEMORY";
        case 0x80070005UL: return "E_ACCESSDENIED";
        case 0x8001010AUL: return "E_NOINTERFACE";
        case 0x80010107UL: return "RPC_E_INVALID_REPLY";
        case 0x800706BAUL: return "RPC_S_SERVER_UNAVAILABLE";
        case 0x800707D8UL: return "HRESULT_FROM_WIN32(ERROR_TIMEOUT)";
        case 0x80010108UL: return "RPC_E_CALL_REJECTED";
        default: return "unknown HRESULT";
    }
}

const char* debug_win32(DWORD err)
{
    static char buf[256];
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, err, 0, buf, (DWORD)sizeof(buf) - 1, NULL);
    if (n == 0) {
        snprintf(buf, sizeof(buf), "Win32 error %lu", (unsigned long)err);
        return buf;
    }
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return buf;
}

const char* debug_hresult(long hr)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "0x%08lX %s", (unsigned long)hr, hresult_str((unsigned long)hr));
    return buf;
}

#endif /* _WIN32 */