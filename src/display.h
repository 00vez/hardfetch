#ifndef HF_DISPLAY_H
#define HF_DISPLAY_H

#include "hf_state.h"

/* Live resolution and refresh rate come from QueryDisplayConfig. Monitor
   name and physical size come from the EDID blob in the registry.
   Where the configuration API refuses, the registry monitors are still
   reported, just without a mode. One entry per display. */
void display_collect(hf_state* s);

#ifdef _WIN32
#include <windows.h>

/* Reads the EDID for one monitor device path. Shared helper. */
int registry_edid(const wchar_t* monitorDevicePath, unsigned char* out, DWORD* outLen);
#endif

#endif
