#include "storage.h"
#include "output.h"

#include <stdio.h>
#include <windows.h>

void print_storage_info(void)
{
    char volume[8] = "C:\\";
    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExA(volume, &freeBytes, &totalBytes, NULL)) {
        double used = (double)(totalBytes.QuadPart - freeBytes.QuadPart);
        double total = (double)totalBytes.QuadPart;
        double pct = total > 0 ? (used / total) * 100.0 : 0;

        const char* unit;
        double divisor;
        if (total >= 1024.0 * 1024.0 * 1024.0 * 1024.0) {
            divisor = 1024.0 * 1024.0 * 1024.0 * 1024.0;
            unit = "TiB";
        } else {
            divisor = 1024.0 * 1024.0 * 1024.0;
            unit = "GiB";
        }
        double usedVal = used / divisor;
        double totalVal = total / divisor;

        char buf[128];
        snprintf(buf, sizeof(buf), "%.2f / %.2f %s (%.0f%%)", usedVal, totalVal, unit, pct);
        print_block("Disk", buf);
    }
}
