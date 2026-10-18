#ifndef HF_MEMORY_SPD_H
#define HF_MEMORY_SPD_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void memory_collect_spd(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS);

#ifdef __cplusplus
}
#endif

#endif
