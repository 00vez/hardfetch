#ifndef HF_MEMORY_SPD_H
#define HF_MEMORY_SPD_H

#ifdef __cplusplus
extern "C" {
#endif

// Returns speed in MHz and sets *outCAS=0 (unavailable via WMI), or -1 on failure
int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS);

#ifdef __cplusplus
}
#endif

#endif
