#include "hf_state.h"

#include <string.h>

void hf_state_init(hf_state* s)
{
    memset(s, 0, sizeof(*s));
    s->cpu_temp_c = -1;
    s->build_ubr = 0;
    strcpy_s(s->os_arch, sizeof(s->os_arch), "x86_64");
    strcpy_s(s->kernel_name, sizeof(s->kernel_name), "WIN32_NT");
}