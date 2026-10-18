#ifndef HF_SHELL_H
#define HF_SHELL_H

#include "hf_state.h"

/* Walks the parent process chain, skipping intermediate shells and conhost,
   instead of looking at a single parent. Falls back to the Windows Terminal
   profile registry and then to the default-terminal delegation GUID. */
void shell_collect(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_shell_info(void);

#endif