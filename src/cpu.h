#ifndef HF_CPU_H
#define HF_CPU_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void cpu_sample_begin(void);
void cpu_sample_end(hf_state* s);
void cpu_collect_static(hf_state* s);
void cpu_collect_live(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_cpu_info(void);

#ifdef __cplusplus
}
#endif

#endif
