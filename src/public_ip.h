#ifndef HF_PUBLIC_IP_H
#define HF_PUBLIC_IP_H

#include "hf_state.h"

/* Blocking HTTPS lookup. Called on the main thread while the worker thread
   is still collecting, then joined with a deadline so -a stays bounded. */
void public_ip_fetch(hf_state* s, unsigned int timeoutMs);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void public_ip_start(void);
void public_ip_print(void);

#endif