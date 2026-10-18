#ifndef HF_MEMORY_H
#define HF_MEMORY_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void memory_collect_usage(hf_state* s);
void memory_collect_spd(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_memory_info(void);

#ifdef __cplusplus
}
#endif

#endif
