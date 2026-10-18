#ifndef HF_KERNEL_H
#define HF_KERNEL_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void kernel_collect_build(hf_state* s);
void kernel_collect_kernel(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_kernel_info(void);

#ifdef __cplusplus
}
#endif

#endif
