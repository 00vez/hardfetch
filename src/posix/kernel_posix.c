#include "../kernel.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <machine/types.h>
#include <sys/sysctl.h>
#endif

void print_kernel_info(void)
{
    char kern[128];
#if defined(__APPLE__)
    /* Darwin kernel version via sysctl kern.osrelease (e.g. 23.5.0) */
    size_t len = sizeof(kern);
    int mib[2] = { CTL_KERN, KERN_OSRELEASE };
    if (sysctl(mib, 2, kern, &len, NULL, 0) != 0)
        strcpy(kern, "Darwin");
    else
        kern[sizeof(kern) - 1] = '\0';
#else
    struct utsname u;
    if (uname(&u) == 0)
        strncpy(kern, u.release, sizeof(kern) - 1);
    else
        strcpy(kern, "N/A");
#endif
    kern[sizeof(kern) - 1] = '\0';
    print_block("Kernel", kern);
}
