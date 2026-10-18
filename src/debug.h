#ifndef HF_DEBUG_H
#define HF_DEBUG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -d / --debug: pure debugging aid, no scope limit.
   Everything goes to stderr, stdout stays clean. */

void debug_enable(void);
int  debug_is_enabled(void);

/* Start/end a module section. debug_end() prints the elapsed milliseconds. */
void debug_begin(const char* module);
void debug_end(void);

/* Emit a line prefixed with the current module name. */
void debug_log(const char* fmt, ...);

/* Raw bytes, hex dump, for EDID / SMBIOS / PDH blobs. */
void debug_raw(const char* label, const void* data, size_t len);

#ifdef _WIN32
const char* debug_win32(unsigned long err);
const char* debug_hresult(long hr);
#endif

#ifdef __cplusplus
}
#endif

#endif