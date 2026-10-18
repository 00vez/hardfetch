#include "../storage.h"
#include "../debug.h"

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

void storage_collect(hf_state* s)
{
    struct statvfs v;
    unsigned long long total, freeb, used;

    debug_begin("disk");

    snprintf(s->disk_volume, sizeof(s->disk_volume), "%s", "/");

    /* The filesystem the executable itself lives on is the closest match to
       what Windows reports for its system drive. */
    {
        char exe[4096];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n > 0) {
            exe[n] = 0;
            /* The mount point is the directory holding the executable, so
               exactly one component comes off. Stripping two turned
               /home/user/bin/hardfetch into /home/u. */
            {
                char* slash = strrchr(exe, '/');
                if (slash == exe) slash[1] = 0;       /* executable in / */
                else if (slash) *slash = 0;
            }
            snprintf(s->disk_volume, sizeof(s->disk_volume), "%.*s",
                     (int)sizeof(s->disk_volume) - 1, exe);
            debug_log("executable path volume '%s'", s->disk_volume);
        }
    }

    if (statvfs(s->disk_volume, &v) != 0) {
        debug_log("statvfs('%s') failed", s->disk_volume);
        s->disk_total = 0;
        s->disk_used = 0;
        debug_end();
        return;
    }

    total = (unsigned long long)v.f_blocks * v.f_frsize;
    freeb = (unsigned long long)v.f_bavail * v.f_frsize;
    if (total < freeb) freeb = total;
    used = total - freeb;

    s->disk_total = total;
    s->disk_used = used;

    debug_log("volume=%s total=%llu used=%llu free=%llu",
              s->disk_volume, total, used, freeb);
    debug_end();
}
