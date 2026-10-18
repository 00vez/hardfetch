#ifndef HF_CPU_TEMP_H
#ifdef __cplusplus
extern "C" {
#endif

#define HF_CPU_TEMP_H

#include "hf_state.h"

/* CPU temperature via the ntdll performance-counter provider
   "Thermal Zone Information" (GUID 52bc5412-dac2-449c-8bc2-96443888fe6b),
   instance _TZ.CPUZ. No COM, no WMI, no timeout. */
void cpu_temp_collect(hf_state* s);

#ifdef __cplusplus

/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_cpu_temp_power(double load);

}
#endif

#endif