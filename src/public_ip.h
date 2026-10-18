#ifndef HF_PUBLIC_IP_H
#define HF_PUBLIC_IP_H

#include "hf_state.h"

/* The lookup never blocks the first frame. It is started as early as
   possible and collected later with a deadline; if nothing came back in time
   the row shows N/A. */
void public_ip_start(hf_state* s, unsigned int timeoutMs);

/* Waits at most waitMs for the lookup. Returns 1 when s->ip_public was
   filled in, 0 when it is still running or failed. */
int  public_ip_join(hf_state* s, unsigned int waitMs);

#endif
