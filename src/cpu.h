#ifndef HF_CPU_H
#define HF_CPU_H

#include "hf_state.h"

/* Opens the CPU load measurement window. Everything collected between
   begin and end becomes the measurement interval, so the window costs
   no wall-clock time of its own. */
void cpu_sample_begin(void);

/* Closes the window and turns it into s->cpu_load (0.0 .. 1.0). */
void cpu_sample_end(hf_state* s);

/* Name, current clock, boost clock, core counts. Does not sleep. */
void cpu_collect_static(hf_state* s);

/* Current clock only; cheap enough for every --live tick. */
void cpu_collect_live(hf_state* s);


/* Legacy entry points, still used by the frozen POSIX build in src/posix/.
   They are implemented there, not in the Windows sources. */
void print_cpu_info(void);

#endif