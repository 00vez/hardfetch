#ifndef HF_GPU_H
#ifdef __cplusplus
extern "C" {
#endif

#define HF_GPU_H

#include "hf_state.h"

/* Adapter list comes from D3DKMT (gdi32, dynamically loaded), which reports
   the PCI address, vendor/device IDs, integrated-vs-discrete state and the
   dedicated memory segment size. WMI Win32_VideoController is not used:
   its AdapterRAM is capped at 4 GB and it reports 0 for most iGPUs.
   Live metrics come from NVML, matched to the adapter by PCI bus id. */
void gpu_collect(hf_state* s);

#ifdef __cplusplus
}
#endif

#endif