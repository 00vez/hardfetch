#include "../kernel.h"
#include "../debug.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/* "Build" on Windows is the UBR patch level. There is no equivalent, so the
   kernel release is not repeated here; the Kernel row carries it instead. */
void kernel_collect_build(hf_state* s)
{
    debug_begin("build");
    s->build_ubr = 0;
    debug_log("no update build revision on this platform");
    debug_end();
}

void kernel_collect_kernel(hf_state* s)
{
    struct utsname u;

    debug_begin("kernel");

    snprintf(s->kernel_name, sizeof(s->kernel_name), "%s", "Linux");
#if defined(__APPLE__)
    snprintf(s->kernel_name, sizeof(s->kernel_name), "%s", "Darwin");
#endif

    if (uname(&u) == 0) {
        s->kernel_major = 0;
        s->kernel_minor = 0;
        snprintf(s->kernel_build_str, sizeof(s->kernel_build_str), "%s", u.release);
    } else {
        s->kernel_build_str[0] = 0;
        debug_log("uname failed");
    }

    debug_log("kernel '%s %s'", s->kernel_name, s->kernel_build_str);
    debug_end();
}
