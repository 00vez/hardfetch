#ifndef HF_GPU_H
#define HF_GPU_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void gpu_collect(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_gpu_info(void);

#ifdef __cplusplus
}
#endif

#endif
