#ifndef HF_DISPLAY_H
#define HF_DISPLAY_H

#include "hf_state.h"

/* Resolution and refresh rate from QueryDisplayConfig; monitor name and
   physical size from the EDID blob in
   HKLM\SYSTEM\CurrentControlSet\Enum\<monitorDevicePath>\Device Parameters\EDID.
   One row per active display. */
void display_collect(hf_state* s);

#endif