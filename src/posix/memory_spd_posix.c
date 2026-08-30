#include "../memory_spd.h"

int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS)
{
    if (outSpeed) *outSpeed = 0;
    if (outCAS)   *outCAS = 0;
    return -1;
}
