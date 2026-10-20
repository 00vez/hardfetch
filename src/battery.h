#ifndef HF_BATTERY_H
#define HF_BATTERY_H

#include "hf_state.h"

/* Battery charge, charging state, temperature and cycle count.
   A platform without a collector leaves have_battery at 0, and the row is
   left out of the output instead of showing a guess. */
void battery_collect(hf_state* s);

#endif
