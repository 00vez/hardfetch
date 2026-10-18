#ifndef HF_HOST_H
#define HF_HOST_H

#include "hf_state.h"

/* The header line needs user and host before anything else renders, on both
   platforms, so both live in the host collector. */
void ident_collect(hf_state* s);

void host_collect(hf_state* s);

#endif
