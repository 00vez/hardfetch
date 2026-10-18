#include "memory.h"
#include "debug.h"

#include <windows.h>

void memory_collect_usage(hf_state* s)
{
    MEMORYSTATUSEX st;

    debug_begin("ram");

    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        debug_log("GlobalMemoryStatusEx failed: %s", debug_win32(GetLastError()));
        s->ram_used = 0;
        s->ram_total = 0;
        debug_end();
        return;
    }

    s->ram_total = (unsigned long long)st.ullTotalPhys;
    s->ram_used = s->ram_total - (unsigned long long)st.ullAvailPhys;

    debug_log("total=%llu used=%llu avail=%llu",
              s->ram_total, s->ram_used, (unsigned long long)st.ullAvailPhys);
    debug_end();
}