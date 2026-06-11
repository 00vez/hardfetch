#ifndef HF_NVML_H
#define HF_NVML_H

// Minimal NVML header for hardfetch (NVIDIA only)

#define NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE 32
#define NVML_DEVICE_PCI_BUS_ID_BUFFER_V2_SIZE 16
#define NVML_DEVICE_NAME_V2_BUFFER_SIZE 96

typedef enum { NVML_SUCCESS = 0 } nvmlReturn_t;
typedef struct nvmlDevice_t* nvmlDevice_t;

typedef enum {
    NVML_TEMPERATURE_GPU = 0,
    NVML_TEMPERATURE_COUNT,
} nvmlTemperatureSensors_t;

typedef struct {
    unsigned int version;
    unsigned long long total;
    unsigned long long reserved;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_v2_t;

enum { nvmlMemory_v2 = (unsigned int)(sizeof(nvmlMemory_v2_t) | (2 << 24U)) };

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef enum {
    NVML_CLOCK_GRAPHICS = 0,
    NVML_CLOCK_SM = 1,
    NVML_CLOCK_MEM = 2,
    NVML_CLOCK_VIDEO = 3,
    NVML_CLOCK_COUNT,
} nvmlClockType_t;

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

#endif
