#ifndef HF_MEMORY_H
#define HF_MEMORY_H

#include "hf_state.h"

/* Total/used from GlobalMemoryStatusEx. Runs on the main thread. */
void memory_collect_usage(hf_state* s);

/* SPD details from a single WMI query. Runs on the worker thread. */
void memory_collect_spd(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_memory_info(void);

#endif