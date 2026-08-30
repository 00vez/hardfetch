# hardfetch v0.2.2 — Apple Silicon M4 Build-Fehler und Fixes (Export)

## Ziel
Universal-Build `hardfetch v0.2.2`: Windows (MSVC /MT) + WSL2/Linux POSIX + macOS Apple Silicon (`__APPLE__` / `arm64`).

## Fehlerhistorie (chronologisch)

### 1. CMakeLists.txt v0.1.0 Windows-only
- `CMakeLists.txt:10`: `message(FATAL_ERROR "hardfetch requires Windows")`; `VERSION 0.1.0`.
- Fix: Austausch mit v0.2.0 POSIX-Zweig (`if(WIN32)` / `else()` + `src/posix/*.c`).
- Commit: `d29860b`.

### 2. `HOST_NAME_MAX`
- `src/main.c:66`: `HOST_NAME_MAX` undeclared auf AppleClang 21.
- Ursache: `<limits.h>` liefert nicht auf macOS.
- Fix: `<sys/param.h>` + `#ifndef HOST_NAME_MAX` `#define HOST_NAME_MAX MAXHOSTNAMELEN`.
- Commit: `6085a3f`.

### 3. `u_int`/`u_char`/`u_short` (sys/types)
- `src/posix/kernel_posix.c`, `uptime_posix.c`, `memory_posix.c`: `sys/sysctl.h` verlangt Typen, Header liefert nicht.
- Fix: `typedef unsigned int u_int;` etc. vor `<sys/sysctl.h>` in allen drei Dateien.
- Commit: `73a73c3`, `9160412`.

### 4. `mach_host_t` → `mach_port_t`
- `memory_posix.c`: `mach_host_t` undefiniert.
- Fix: `mach_port_t host = mach_host_self();`.
- Commit: `4993d3e`.

### 5. `IFF_UP`/`IFF_LOOPBACK`
- `ip_addr_posix.c`: Konstanten nicht in `<net/if.h>` auf macOS.
- Fix: `#ifndef IFF_UP #define IFF_UP 0x1 #endif` etc.
- Commit: `8cf79a8`.

### 6. `CMakeLists.txt` POSIX-Link libs (pthread, dl)
- `build-silicon`: Link gegen `pthread dl`; `gpu_posix.c`/`cpu_posix.c` benötigen `sys/sysctl.h` + Typedefs.

### 7. `sysctlbyname` + `sys/sysctl.h` in `gpu_posix.c` / `cpu_posix.c`
- `gpu_posix.c`: `sysctlbyname` undeclared → `<sys/sysctl.h>` + Typedefs.
- Fix: `typedef` + Include (`07385f3`).

### 8. `sysctlbyname` `hw.cpufrequency` liefert 0 / ENOENT auf M4
- `sysctl -n hw.cpufrequency` leer; `hw.cpufrequency_max` leer; `machdep.cpu.frequency` unknown.
- **Kernursache**: Apple Silicon hat `sysctl`-Frequenz nicht implementiert (Intel-only Key). `libvirt` zeigt dasselbe Problem (`sysctl` liefert 0, reported als 0).
- Fix: `pmgr`-Node (`IOServiceGetMatchingService`) + `voltage-states5-sram` (P-Cores) / `1` (E-Cores) / `9` (GPU) als `CFData`-Blob, Heuristik `>100000000` = Hz (M1/M3), sonst kHz (M4).
- Implementiert in `cpu_posix.c`: `appleMaxFreqMHz()`.
- Implementiert in `gpu_posix.c`: `appleMaxFreqMHz("voltage-states9-sram")` → `Apple M4 (10) @ 1.47 GHz [Integrated]`.

### 9. CPU-Name / GPU-Name
- `cpu_posix.c`: `sysctlbyname("machdep.cpu.brand_string")` liefert `Apple M4 (10)`.
- `gpu_posix.c`: `appleMaxFreqMHz` liefert Clock; Name `Apple M4 (10)` als Hard-Fallback (kein `IOName`-Key für Apple iGPU).

### 10. Version-Bump vergessen
- `92216a6`: `VERSION` blieb `0.2.1`; Fix `daf069a`: `VERSION 0.2.2` in `main.c` + `CMakeLists.txt`.

### 11. `memory_spd_posix.c` `sysctl` + Typedefs + Link
- `sys/sysctl.h` ohne Typedefs → Fehler; gefixt.
- `CMakeLists.txt`: `target_link_libraries(hardfetch PRIVATE "-framework CoreFoundation" "-framework IOKit")` für `appleMaxFreqMHz`.

### 12. `memory_spd_posix.c` RAM-Freq
- `sysctlbyname("hw.memfrequency")` liefert `0` auf M4; bleibt `N/A`. Keine public Alternative.

### 13. `cpu_temp_posix.c` AppleSMC
- `AppleARMPMUTempSensor` gefunden (`ioreg -r -c`), aber keine `Temperature`-Property via `IORegistryEntryCreateCFProperty`. Fastfetch zeigt `N/A` oder nutzt interne API.
- Fix: `TC0D`-Versuch (nicht verfügbar auf M4) → `N/A` korrekt.

### 14. Warnungen (2 Stück)
- `memory_spd_posix.c`: `kb_from_line` ungenutzt; `uint64_t*` → `vm_size_t*` in `memory_posix.c`.

## Aktueller Zustand v0.2.2
- `build-silicon/hardfetch`: `v0.2.2`
- `CPU Apple M4 (10) @ 4.46 GHz` ✅ (pmgr)
- `GPU Apple M4 (10) @ 1.47 GHz [Integrated]` ✅ (pmgr)
- `Temp N/A` ✅ (kein public Key — korrekt)
- `RAM-Freq N/A` ✅ (kein sysctl — korrekt)
- Keine Build-Fehler; 2 Warnungen (harmless)

## Verbleibende Lücken (public APIs)
- **CPU-Temp**: `AppleARMPMUTempSensor` hat kein `Temperature`-Property über `IORegistryEntryCreateCFProperty`; `fastfetch` nutzt evtl. `powermetrics` oder `IOReport` (private). Ohne private APIs nicht lösbar.
- **RAM-Freq**: Keine `sysctl`-Alternative auf M4.
- **GPU-Details** (`Core`, `Mem`, `Load`, `Temp`, `Power`): Apple-IOKit liefert nur `IOName` + `pmgr`-Clock; zusätzliche Stats nicht public für iGPU.

## Referenzen / Quellen
- `fastfetch` `src/detection/cpu/cpu_apple.c`: `pmgr` + `voltage-states*-sram`, `CFData`-Heuristik (`>100M` Hz / kHz).
- `fastfetch` GitHub: https://github.com/fastfetch-cli/fastfetch
- `eclecticlight.co`: Apple Silicon CPU-Frequenzen (M4 Pro/Max Variants).
- `patchew.org`: `sysctl` liefert 0 auf Apple Silicon (Libvirt-Report).
- `fossies.org`: `cpu_apple.c`, `gpu_apple.c` (public IOKit + `pmgr`-Parsen).

## Reproduktion (Mac)
```zsh
git clone https://github.com/00vez/hardfetch.git
cmake -B build-silicon -S .
cmake --build build-silicon
./build-silicon/hardfetch -v   # v0.2.2
./build-silicon/hardfetch -n   # Full
```

## Dateien geändert (final)
- `src/main.c`: `VERSION 0.2.2`
- `CMakeLists.txt`: `VERSION 0.2.2` + Apple Link (`-framework CoreFoundation IOKit`)
- `src/posix/cpu_posix.c`: `appleMaxFreqMHz` (`pmgr`), `sysctlbyname` Brand
- `src/posix/gpu_posix.c`: `appleMaxFreqMHz` (`pmgr`), `IOService` Name
- `src/posix/cpu_temp_posix.c`: `AppleSMC` Versuch (`TC0D`)
- `src/posix/memory_spd_posix.c`: `sysctl` `hw.memfrequency`
- `progress.html`: Dokumentation v0.2.2 Fehler/Fixed
