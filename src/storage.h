#ifndef HF_STORAGE_H
#define HF_STORAGE_H

#include "hf_state.h"

/* Resolves the Windows install drive from the executable path, so a
   non-C: install is reported correctly. */
void storage_collect(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_storage_info(void);

#endif