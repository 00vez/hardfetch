#include <sys/types.h>
#include <sys/sysctl.h>
#include "../memory_spd.h"

int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS)
{
    if (outSpeed) *outSpeed = 0;
    if (outCAS)   *outCAS = 0;
    int mhz = 0;
    size_t len = sizeof(mhz);
    if (sysctlbyname("hw.memfrequency", &mhz, &len, NULL, 0) == 0 && mhz > 0) {
        if (outSpeed) *outSpeed = (unsigned int)mhz;
        return 0;
    }
    return -1;
}
