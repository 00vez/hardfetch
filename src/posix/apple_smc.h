#ifndef APPLE_SMC_H
#define APPLE_SMC_H
#include <stdio.h>
int apple_smc_read_temp(const char *key, int *out_c);
int apple_smc_read_int(const char *key, int *out);
int apple_smc_cpu_temp(int *out_c);
void apple_smc_dump(FILE *out);
#endif
