#ifndef HF_EDID_H
#define HF_EDID_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EDID 1.3/1.4 blocks as stored in the Windows registry and in the sysfs
   edid nodes under Linux. Shared by both collectors. */

#define EDID_BLOCK_SIZE 128

int  edid_is_valid(const unsigned char* e, size_t len);

/* 0xFC descriptor: model name, 13 bytes, newline terminated.
   Falls back to the manufacturer and model number as "XXX-NNNN". */
void edid_name(const unsigned char* e, char* out, size_t outLen);

/* Physical size in millimetres. Some panels report nonsense in the detailed
   timing descriptor, so it is cross checked against the basic display
   parameters the same way fastfetch does it. */
void edid_physical_size(const unsigned char* e, unsigned int* wMm, unsigned int* hMm);

/* Deliberately no resolution or refresh helper here. A base block without
   the CTA extension carries only one detailed timing descriptor, often a
   fallback mode, so any refresh rate derived from it would be invented.
   Resolution and rate come from the platform's live mode API instead. */

#ifdef __cplusplus
}
#endif

#endif