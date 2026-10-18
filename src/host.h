#ifndef HF_HOST_H
#define HF_HOST_H

#include "hf_state.h"

void host_collect(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_host_info(void);

#endif