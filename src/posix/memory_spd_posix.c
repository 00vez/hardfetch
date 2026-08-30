#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#include "../memory_spd.h"

int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS)
{
    if (outSpeed) *outSpeed = 0;
    if (outCAS)   *outCAS = 0;
#if defined(__APPLE__)
    int mhz = 0;
    size_t len = sizeof(mhz);
    if (sysctlbyname("hw.memfrequency", &mhz, &len, NULL, 0) == 0 && mhz > 0) {
        if (outSpeed) *outSpeed = (unsigned int)mhz;
        return 0;
    }
#endif
    return -1;
}
