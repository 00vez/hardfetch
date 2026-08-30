#include "apple_smc.h"
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define SMC_METHOD 2
#define SMC_CMD_READ_KEY 5
#define SMC_CMD_KEY_BY_INDEX 8
#define SMC_CMD_KEY_INFO 9

typedef struct {
  UInt32 dataSize;
  UInt32 dataType;
  UInt8  dataAttributes;
} __attribute__((packed)) SmcKeyInfo;

#pragma pack(push, 1)
typedef struct {
  UInt32     key;
  UInt8      vers[2];
  UInt8      plimit[16];
  UInt8      padding[10];
  SmcKeyInfo keyInfo;
  UInt8      result;
  UInt8      status;
  UInt8      data8;
  UInt32     data32;
  UInt8      bytes[32];
} SmcParam;
#pragma pack(pop)

static UInt32 fourcc(const char *s) {
  return ((UInt32)(UInt8)s[0] << 24) | ((UInt32)(UInt8)s[1] << 16) |
         ((UInt32)(UInt8)s[2] << 8) | (UInt32)(UInt8)s[3];
}

static kern_return_t smc_call(io_connect_t c, SmcParam *p) {
  size_t sz = sizeof(SmcParam);
  return IOConnectCallStructMethod(c, SMC_METHOD, p, sz, p, &sz);
}

static io_connect_t smc_open(void) {
  io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("AppleSMC"));
  if (!svc) return 0;
  io_connect_t conn = 0;
  if (IOServiceOpen(svc, mach_task_self(), 0, &conn) != KERN_SUCCESS) conn = 0;
  IOObjectRelease(svc);
  return conn;
}

typedef struct { UInt32 type; UInt32 size; UInt8 bytes[32]; } SmcVal;

static bool smc_read(io_connect_t c, const char *key, SmcVal *out) {
  SmcParam p = {0};
  p.key = fourcc(key);
  p.data8 = SMC_CMD_KEY_INFO;
  if (smc_call(c, &p) != KERN_SUCCESS || p.result != 0) return false;
  out->type = p.keyInfo.dataType;
  out->size = p.keyInfo.dataSize;
  p.data8 = SMC_CMD_READ_KEY;
  p.keyInfo.dataSize = out->size;
  if (smc_call(c, &p) != KERN_SUCCESS || p.result != 0) return false;
  memcpy(out->bytes, p.bytes, 32);
  return true;
}

static bool smc_decode(const SmcVal *v, double *out) {
  if (v->type == fourcc("flt ") && v->size == 4) {
    UInt32 raw = OSSwapBigToHostInt32(*(const UInt32 *)v->bytes);
    float f; memcpy(&f, &raw, 4);
    *out = f; return true;
  }
  if (v->type == fourcc("sp78") && v->size == 2) {
    *out = (int8_t)v->bytes[0] + v->bytes[1] / 256.0; return true;
  }
  if (v->type == fourcc("fpe2") && v->size == 2) {
    *out = (double)OSSwapBigToHostInt16(*(const UInt16 *)v->bytes) / 4.0; return true;
  }
  if (v->type == fourcc("ui32") && v->size == 4) {
    *out = (double)OSSwapBigToHostInt32(*(const UInt32 *)v->bytes); return true;
  }
  if (v->type == fourcc("ui8 ") && v->size == 1) { *out = v->bytes[0]; return true; }
  return false;
}

int apple_smc_read_temp(const char *key, int *out_c) {
  io_connect_t c = smc_open();
  if (!c) return -1;
  SmcVal v;
  bool ok = smc_read(c, key, &v);
  double d = 0;
  int ret = -1;
  if (ok && smc_decode(&v, &d) && d > -40 && d < 125) { *out_c = (int)(d + 0.5); ret = 0; }
  IOServiceClose(c);
  return ret;
}

int apple_smc_read_int(const char *key, int *out) { return apple_smc_read_temp(key, out); }

int apple_smc_cpu_temp(int *out_c) {
  io_connect_t c = smc_open();
  if (!c) return -1;
  SmcVal v;
  if (!smc_read(c, "#KEY", &v) || v.size != 4) { IOServiceClose(c); return -1; }
  UInt32 count = OSSwapBigToHostInt32(*(const UInt32 *)v.bytes);
  double best = -1e9;
  bool found = false;
  for (UInt32 i = 0; i < count; i++) {
    SmcParam p = {0};
    p.data8 = SMC_CMD_KEY_BY_INDEX;
    p.data32 = OSSwapHostToBigInt32(i);
    if (smc_call(c, &p) != KERN_SUCCESS || p.result != 0) continue;
    char name[5] = {(char)(p.key >> 24), (char)(p.key >> 16), (char)(p.key >> 8), (char)p.key, 0};
    // prefix filter t/T
    if (name[0] != 'T' && name[0] != 't') continue;
    if (!smc_read(c, name, &v)) continue;
    double d;
    if (!smc_decode(&v, &d)) continue;
    if (d < 5 || d > 120) continue;
    if (d > best) { best = d; found = true; }
  }
  IOServiceClose(c);
  if (!found) return -1;
  *out_c = (int)(best + 0.5);
  return 0;
}

void apple_smc_dump(FILE *out) {
  io_connect_t c = smc_open();
  if (!c) { fprintf(out, "# kein AppleSMC\n"); return; }
  SmcVal v;
  if (!smc_read(c, "#KEY", &v) || v.size != 4) {
    fprintf(out, "# #KEY nicht lesbar -> Protokollfehler\n"); IOServiceClose(c); return;
  }
  UInt32 count = OSSwapBigToHostInt32(*(const UInt32 *)v.bytes);
  fprintf(out, "# total keys: %u\n", (unsigned)count);
  for (UInt32 i = 0; i < count; i++) {
    SmcParam p = {0};
    p.data8 = SMC_CMD_KEY_BY_INDEX;
    p.data32 = OSSwapHostToBigInt32(i);
    if (smc_call(c, &p) != KERN_SUCCESS || p.result != 0) continue;
    char name[5] = {(char)(p.key >> 24), (char)(p.key >> 16), (char)(p.key >> 8), (char)p.key, 0};
    if (!smc_read(c, name, &v)) continue;
    char type[5] = {(char)(v.type >> 24), (char)(v.type >> 16), (char)(v.type >> 8), (char)v.type, 0};
    double d;
    fprintf(out, "%-4s %s %u ", name, type, (unsigned)v.size);
    if (smc_decode(&v, &d)) fprintf(out, "%g\n", d);
    else fprintf(out, "-\n");
  }
  IOServiceClose(c);
}
