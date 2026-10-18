#ifndef HF_OS_INFO_H
#define HF_OS_INFO_H

#include "hf_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows collectors. */

void os_collect(hf_state* s);

/* Legacy entry points, still used by the frozen POSIX build in src/posix.
   They are implemented there, not in the Windows sources. */

void print_os_info(void);
const char* get_os_product(void);
const char* get_os_build(void);
const char* get_os_display_version(void);

#ifdef __cplusplus
}
#endif

#endif
