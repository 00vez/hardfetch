#ifndef HF_MEMORY_SPD_H
#ifdef __cplusplus
extern "C" {
#endif

#define HF_MEMORY_SPD_H

#include "hf_state.h"

/* One WMI query against Win32_PhysicalMemory fills speed, CAS, type,
   slot count and per-module size. SMBIOS is not reachable on this platform:
   both EnumSystemFirmwareTables('RSMB') and
   NtQuerySystemInformation(SystemFirmwareTableInformation) return empty. */
void memory_collect_spd(hf_state* s);

#ifdef __cplusplus

/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS);

}
#endif

#endif