#ifndef HF_IP_ADDR_H
#define HF_IP_ADDR_H

#include "hf_state.h"

/* Best interface: highest-scoring operational adapter, preferring the one
   that carries the default route. With show_all, every remaining active
   adapter is appended to s->ip_extra. */
void ip_collect(hf_state* s, int show_all);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_ip_info(void);

#endif