#include "apple_smc.h"
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

#pragma pack(push, 1)

typedef struct {
    UInt32 dataSize;
    UInt32 dataType;
    UInt8  dataAttributes;
} SmcKeyInfo;

typedef struct {
    UInt32     key;
    UInt8      vers[4];
    UInt8      plimit[16];
    SmcKeyInfo keyInfo;
    UInt8      result;
    UInt8      status;
    UInt8      data8;
    UInt32     data32;
    UInt8      bytes[32];
} SmcParam;

#pragma pack(pop)

static kern_return_t smc_call(io_connect_t conn, SmcParam* p) {
    size_t size = sizeof(SmcParam);
    return IOConnectCallStructMethod(conn, 2, p, size, p, &size);
}

static UInt32 fourcc(const char* s) {
    return ((UInt32)(unsigned char)s[0] << 24) |
           ((UInt32)(unsigned char)s[1] << 16) |
           ((UInt32)(unsigned char)s[2] << 8)  |
           ((UInt32)(unsigned char)s[3]);
}

static int apple_smc_read_key(io_connect_t conn, const char* key_name, int* temp_c) {
    int res = -1;
    SmcParam p = {0};
    p.key = fourcc(key_name);
    p.data8 = 5; /* Read Key */
    if (smc_call(conn, &p) == KERN_SUCCESS && p.result == 0) {
        SmcKeyInfo info = p.keyInfo;
        int size = (int)info.dataSize;
        if (size > 0 && size <= 32) {
            int val = 0;
            if (info.dataType == fourcc("flt ")) {
                UInt32 raw = 0;
                memcpy(&raw, p.bytes, sizeof(UInt32));
                raw = OSSwapBigToHostInt32(raw);
                float f = 0.0f;
                memcpy(&f, &raw, sizeof(float));
                val = (int)(f * 100.0); /* approximate °C * 100 for int */
            } else if (info.dataType == fourcc("sp78")) {
                val = (int)(p.bytes[0] + p.bytes[1] / 256.0);
            } else {
                /* Fallback: treat as integer (raw) */
                val = (int)(p.bytes[0] | (p.bytes[1] << 8));
            }
            if (val > -40 && val < 120) {
                *temp_c = val;
                res = 0;
            }
        }
    }
    return res;
}

int apple_smc_read_int(const char* key_name, int* out_val) {
    int temp = 0;
    if (apple_smc_read_temp(key_name, &temp) == 0) {
        *out_val = temp;
        return 0;
    }
    return -1;
}

int apple_smc_read_temp(const char* key_name, int* temp_c) {
    io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceNameMatching("AppleSMC"));
    if (!svc) return -1;
    io_connect_t conn = 0;
    int ok = -1;
    if (IOServiceOpen(svc, mach_task_self(), 0, &conn) == KERN_SUCCESS) {
        ok = apple_smc_read_key(conn, key_name, temp_c);
        IOServiceClose(conn);
    }
    IOObjectRelease(svc);
    return ok;
}
