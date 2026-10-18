#include "hf_state.h"

#include <stdio.h>
#include <string.h>

void hf_state_init(hf_state* s)
{
    memset(s, 0, sizeof(*s));
    s->cpu_temp_c = -1;
    s->build_ubr = 0;
#ifdef _WIN32
    strcpy_s(s->os_arch, sizeof(s->os_arch), "x86_64");
#else
    snprintf(s->os_arch, sizeof(s->os_arch), "%s", "x86_64");
#endif
#ifdef _WIN32
    strcpy_s(s->kernel_name, sizeof(s->kernel_name), "WIN32_NT");
#else
    snprintf(s->kernel_name, sizeof(s->kernel_name), "%s", "Linux");
#endif
}