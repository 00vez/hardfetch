#include "battery.h"
#include "debug.h"

/* Windows laptops have a battery, but there is no collector for it yet.
   The row stays out of the output instead of showing a guess. */
void battery_collect(hf_state* s)
{
    (void)s;
    debug_log("battery: no collector on Windows yet");
}
