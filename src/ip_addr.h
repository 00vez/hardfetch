#ifndef HF_IP_ADDR_H
#define HF_IP_ADDR_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void ip_collect(hf_state* s, int show_all);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_ip_info(void);

#ifdef __cplusplus
}
#endif

#endif
