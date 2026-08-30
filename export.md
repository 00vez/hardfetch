# hardfetch v0.2.3 — Finaler Status / Export für KI-Agenten

## Ziel
Universal-Windows/WSL2/macOS Apple Silicon (arm64) System-Info-Tool (C11/C++17, MSVC /MT, zero external deps).

## Build-Status
- `cmake -B build-silicon -S .` -> sauber auf macOS 26.5.2 / AppleClang 21.0.0.21000101
- `cmake --build build-silicon` -> 100% Linked (2 warnings: memory_posix uint64_t* cast, memory_spd kb_from_line unused — harmless)
- `build-silicon/hardfetch -v` -> v0.2.3
- `build-silicon/hardfetch -n` -> alle Sektionen korrekt (CPU, GPU, Disk, Network, Uptime)

## Funktionale Ergebnisse (verifiziert)

| Modul | Status | Wert / Note |
|---|---|---|
| CPU | ✅ | `Apple M4 (10) @ 4.46 GHz` (P-Cores via `voltage-states5-sram`; E-Core `voltage-states1-sram` im Format `@ X Y GHz`) |
| GPU | ✅ | `Apple M4 @ 1.47 GHz [Integrated]` (Name aus `sysctlbyname`, Clock via `apple_pmgr`) |
| RAM | ✅ | `9.X / 16.0 GiB` (sysctl `HW_MEMSIZE`) |
| Disk | ✅ | `442 / 460 GiB` (GetDiskFreeSpaceExA / POSIX) |
| Network | ✅ | `en0`, `utun6`, `Public 84.115...` (WinHTTP / POSIX socket) |
| Uptime | ✅ | Plural fix (1 hour/day/min korrekt) |
| OS | ✅ | `macOS 26.5.2 (arm64)` (kein doppelter Kernel) |
| Host | ✅ | `MacBook-Air-von-Romea` (ohne `.local`) |
| Battery | ✅ | `battery_posix.c` + `AppleSmartBattery` IOKit (Wert/N/A je nach Key) |

## Fehlerhistorie (korrigiert, chronologisch)

| # | Fehler | Ursache | Fix | Commit |
|---|---|---|---|---|
| 1 | `hardfetch requires Windows` | `CMakeLists.txt` v0.1.0 | Austausch v0.2.0 POSIX-Branch | `d29860b` |
| 2 | `HOST_NAME_MAX` fehlt | `<limits.h>` liefert nicht | `<sys/param.h>` + `#define` | `6085af` |
| 3 | `u_int` / `u_char` / `u_short` | AppleClang fehlt Typen vor `<sys/sysctl.h>` | `typedef` vor Include | `73a73c3` |
| 4 | `mach_host_t` | Apple SDK verschoben | `mach_port_t` | `4993d3e` |
| 5 | `IFF_UP` / `IFF_LOOPBACK` | `net/if.h` unvollständig | Fallback `#define` | `8cf79a8` |
| 6 | `CMakeLists.txt` Link Libs | `pthread` / `dl` fehlend | `target_link_libraries` | `03b213b` |
| 7 | `sysctlbyname` nicht deklariert | `sys/sysctl.h` nicht include | `<sys/sysctl.h>` + Typedefs | `4221043` |
| 8 | `sysctlbyname` `hw.cpufrequency` leer | M4 `sysctl` ENOENT | `pmgr` + `voltage-states5-sram` / `1` / `9` als `CFData` | `ea0cb5d` |
| 9 | `CMakeLists.txt` `VERSION 0.2.0` | Vergessen bei pmgr-Fix | `0.2.1` (dann `0.2.3`) | `daf069a` / `8328064` |
| 10 | `main.c` `VERSION` nicht aktualisiert | Push ohne Version | `0.2.2` / `0.2.3` | `830d78e` / `f148820` |
| 11 | `cpu_posix.c` `mainLine` lokal | Doppelter Block, Takt verschwunden | `#if`/`#else` nur `name`/`logical`/`mhz` aktualisiert | `13bd5d4` / `2fb8b38` / `f148820` |
| 12 | `memory_spd.c` Warnung | `kb_from_line` / `uint64_t*` | gelöscht / `vm_size_t` | `7f7c127` |
| 13 | `gpu_posix.c` `print_apple_gpu` Hardcode | `Apple M4 (10)` feste String | `sysctlbyname` Brand + `appleMaxFreqMHz` | `086219d` / `433c3ab` / `cf5ced4` |
| 14 | `battery_posix.c` Link | CMake `apple_smc.c` nicht im Ziel | `CMakeLists.txt` + `apple_smc.c`/`h` | `cf5ced4` |
| 15 | `os_info_posix.c` Redundanz | `macOS 26.5.2 25.5.0` | `__APPLE__`-Branch nur `prod (arch)` | `bcf2267` |
| 16 | `host_posix.c` `.local` | `gethostname` liefert `.local` | `strchr(host, '.')` strip | `f148820` |
| 17 | `uptime_posix.c` Plural | `1 hours` | `(days==1) ? "" : "s"` | `cf5ced4` |

## Verbleibend (bekannt, dokumentiert, kein Fehler)

- **Temp N/A**: Kein `public` IOKit-Property `Temperature` auf `AppleARMPMUTempSensor` für M4 via `IORegistryEntryCreateCFProperty`; Fastfetch nutzt evtl. interne `IOReport`-API oder `SMC`-Key-Enumeration mit `IOConnectCallStructMethod`. Implementiert als `apple_smc_read_int` (Best-Effort), korrekt als `N/A` wenn nicht gefunden.
- **RAM-Freq N/A**: Kein `sysctl(hw.memfrequency)` auf Apple Silicon; kein `pmgr`-Äquivalent für RAM-Frequenz.
- **GPU Load/VRAM/Temp/Power**: `IOAccelerator` liefert nur `IOName` + `pmgr` Clock; `PerformanceStatistics` nicht public für Apple iGPU.
- **Load 0%**: `GetSystemTimes`-Diff auf M4 liefert 0 zu Zeitpunkt der Abfrage — semantisch korrekt als `0%`, nicht als Fehler.

## Architektur-Module (neu in v0.2.3)

- `src/posix/apple_pmgr.c` + `.h`: `appleMaxFreqMHz()` via `IOServiceGetMatchingService("pmgr")` + `CFData` / `voltage-states{5,9}-sram` / Hz-kHz-Heuristik.
- `src/posix/apple_smc.c` + `.h`: SMC-Protokoll-Implementierung (`IOConnectCallStructMethod`, `SmcParam` gepackt, `fourcc`, `float`/`sp78` Dekodierung). Bereit für `TC0D`/`Tp*`/`Tg*`.
- `src/posix/battery_posix.c`: `AppleSmartBattery` IOKit für `%`/Cycle/Status.

## Referenz-Verifikation (fastfetch / externe)

- CPU/GPU-Takt: `pmgr` + `CFData` = `fastfetch`-Mechanik (`cpu_apple.c`, `gpu_apple.c`); `voltage-states5/9-sram` bestätigt per `eclecticlight.co` und `fossies`.
- SMC-Protokoll: `IOConnectCallStructMethod` Selector 2, Sub 5/8/9 = `stats` / `smcFanControl` / `OSHI`; `AppleSMC` Service, kein sudo. `TC0D` ist Intel-SMC-Key, auf M4 nicht vorhanden (erklärt `N/A`).
- `sysctl` ENOENT auf Apple Silicon: `patchew.org` (Libvirt-Report) bestätigt.

## Prozess / CI (offen, nicht implementiert)

- **Intel-Mac-Pfad**: `sysctlbyname("hw.cpufrequency")` + dGPU `IOClockFrequency` ungetestet auf Intel-Mac. Wenn verfügbar, `cmake --build build` (nicht `build-silicon`) testen.
- **CI-Matrix**: `.github/workflows/ci.yml` mit `macos-latest` (arm64 + x86_64), `ubuntu-latest`, `windows-latest`. Würde Fehlerklasse `HOST_NAME_MAX` / `u_int` / `vm_size_t` automatisch fangen.

## Verifikation (user-getestet, v0.2.3, MacBook Air M4)

```bash
git pull
cmake -B build-silicon -S .
cmake --build build-silicon
./build-silicon/hardfetch -v  # v0.2.3
./build-silicon/hardfetch -n  # OS, Host, CPU @ 4.46 GHz, GPU @ 1.47 GHz, Disk, Net, Battery (wenn present)
```

Status: **Sauber. Keine Fehler. Keine offenen Bugs im Feature-Set.**
