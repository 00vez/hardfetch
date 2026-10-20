#include "battery.h"
#include "debug.h"

#include <windows.h>

/* GetSystemPowerStatus reports the charge in percent and whether the
   machine runs on mains. It works on laptops without WMI and without a
   battery class driver, and it reports 255 for the percent on a desktop
   without a battery. */
void battery_collect(hf_state* s)
{
    SYSTEM_POWER_STATUS st;

    debug_begin("battery");

    if (!GetSystemPowerStatus(&st)) {
        debug_log("GetSystemPowerStatus failed");
        debug_end();
        return;
    }

    /* 255 means unknown, which is the case on a desktop. */
    if (st.BatteryLifePercent == 255) {
        debug_log("no battery");
        debug_end();
        return;
    }
    if (st.BatteryFlag == 255) {
        debug_log("battery status unknown");
        debug_end();
        return;
    }

    s->have_battery = 1;
    s->battery_pct = st.BatteryLifePercent;
    /* 1 means running on mains. */
    s->battery_charging = (st.ACLineStatus == 1) ? 1 : 0;

    debug_log("battery %u%% charging=%d", s->battery_pct, s->battery_charging);
    debug_end();
}
