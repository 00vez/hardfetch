#include "uptime.h"
#include "debug.h"

#include <windows.h>

void uptime_collect(hf_state* s)
{
    ULONGLONG ms = GetTickCount64();
    s->uptime_secs = ms / 1000ULL;
    s->uptime_secs_live = s->uptime_secs;
    debug_log("GetTickCount64 = %llu ms -> %llu s", ms, s->uptime_secs);
}