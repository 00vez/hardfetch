#ifndef HF_CPU_TEMP_H
#define HF_CPU_TEMP_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void cpu_temp_collect(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_cpu_temp_power(double load);

#ifdef __cplusplus
}
#endif

#endif
