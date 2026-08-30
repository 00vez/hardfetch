=== hardfetch v0.2.4 Export ===

BUILD: cmake -B build-silicon -S . && cmake --build build-silicon (AppleClang 21, 0 warnings target)
RUN: hardfetch v0.2.4 (macOS 26.5.2 arm64, Apple M4)

VERIFIED:
- CPU  Apple M4 (10) @ 4.46 GHz E 2.85 GHz (pmgr voltage-states5/1-sram, ComputerBase cross-checked)
- GPU  Apple M4 @ 1.47 GHz [Integrated] (pmgr voltage-states9-sram)
- OS   macOS 26.5.2 (arm64)
- Host MacBook-Air-von-Romea (no .local)
- Uptime plural fixed (1 hour/day/min)
- Battery module present (AppleSmartBattery, not yet linked in main output)
- Disk/Network/Uptime clean

FIXES v0.2.4 (all 6):
1. apple_smc.c rewrite: packed SmcParam 74B, SMC_METHOD 2, sub 5/8/9, fourcc, OSSwapBigToHost, flt/sp78/fpe2/ui32 decode, result==0 check
2. -d / --smc-dump flag (short -d per request) -> apple_smc_dump(stdout), returns 0
3. cpu_temp_posix.c: is_arm64() sysctl hw.optional.arm64 -> apple_smc_cpu_temp() discovery max; Intel fallback TC0D/TC0P (sp78)
4. Load: macOS host_processor_info PROCESSOR_CPU_LOAD_INFO delta 200ms, vm_deallocate, Load %.1f%% / N/A (was 20ms GetSystemTimes/proc-stat)
5. Version bump 0.2.3 -> 0.2.4 (CMake + main.c)
6. Shared apple_pmgr.c (no duplicate), GPU label via brand_string (no hardcoded (10))

OPEN / NEXT:
- Temp: dump must run on MBA M4 to confirm t-prefix keys (tdie/tdev vs Tp). Filter t/T 5-120C max is implemented, but real sensor names only known after dump.
- Battery/Display/GPU-Load: battery_posix.c exists but not in main flow; Display/CoreGraphics and PerformanceStatistics still TODO
- Intel path (sysctl hw.cpufrequency, dGPU) unverified
- CI matrix missing

VERIFY ON MBA M4:
  git pull && rm -rf build-silicon && cmake -B build-silicon -S . && cmake --build build-silicon
  build-silicon/hardfetch -v  # v0.2.4
  build-silicon/hardfetch -d > smc-mba-m4.txt; wc -l smc-mba-m4.txt; grep -E '^[Tt]' smc-mba-m4.txt | sort
  build-silicon/hardfetch | grep -E 'CPU|Load|Temp'
  # Temp should be +-10C of: sudo powermetrics --samplers cpu_power -i 1000 | grep -i "die temperature"
  # Load: yes > /dev/null & -> hardfetch should show >50%
