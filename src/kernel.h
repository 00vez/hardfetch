#ifndef HF_KERNEL_H
#define HF_KERNEL_H

#include "hf_state.h"

/* Registry CurrentBuildNumber.UBR, shown as the "Build" row. */
void kernel_collect_build(hf_state* s);

/* PEB version triple + UBR, shown as the "Kernel" row. */
void kernel_collect_kernel(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_kernel_info(void);

#endif