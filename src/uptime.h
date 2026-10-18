#ifndef HF_UPTIME_H
#define HF_UPTIME_H

#include "hf_state.h"

/* GetTickCount64 does not wrap for any practical uptime; the 32-bit
   GetTickCount does, which is why this uses the 64-bit variant. */
void uptime_collect(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_uptime_info(void);

#endif