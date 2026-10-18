#ifndef HF_UPTIME_H
#define HF_UPTIME_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void uptime_collect(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_uptime_info(void);

#ifdef __cplusplus
}
#endif

#endif
