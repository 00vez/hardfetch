# hardfetch — agent instructions

Cross-platform `neofetch`-style tool. Windows, Linux/WSL2 and macOS share the
same output. C11 for `.c`, C++17 only where WMI/COM is needed
(`memory_spd.cpp`, `cpu_temp.cpp`, `gpu.cpp`). MSVC `/MT`, statically
linked, 216 KB, no external runtime dependencies.

## Build

```powershell
# Windows (VS 2022 x64)
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\hardfetch.exe
```
```bash
# Linux / WSL2
cmake -B build-wsl -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-wsl -j4
./build-wsl/hardfetch
```
```bash
# macOS, both architectures on one machine
cmake -B build-arm64 -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-arm64 -j4
cmake -B build-x64 -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build-x64 -j4
```

`-DHF_ASAN=ON` builds a sanitiser variant. Its DLL sits next to the MSVC
tools; it is not on PATH by default.

**Always build clean before signing off.** Incremental builds reuse object
files, and a stale `output.h` or `hf_state.h` hides warnings and layout
mismatches. Two warnings and one shared-header breakage only appeared with
`--clean-first`.

`.github/workflows/macos.yml` builds and runs the macOS binaries on a real
Mac. It is the only place the Apple paths are compiled, and the output of
the runner is the evidence that they work.

## Output

One renderer for every platform: `render.c`, `fmt.c`, `edid.c`, `output.c`,
`debug.c`, `hf_state.c`, `main.c`. Changing layout means changing it once.

- Label column at 14, detail lines at 13, separator `|`, U+00B7 for degree
- Every collector fills `hf_state`; nothing prints directly
- Static fields are collected once, volatile ones per `--live` tick
- A field the platform cannot supply is omitted. Never invent it

## Flags

`-h -v -a -l[secs] -d`. `--net`/`-n` are gone. `-d` is `--debug`, so the
macOS SMC dump is long-only (`--smc-dump`). `-l` needs a real terminal; with
redirected output it prints one frame and exits rather than looping forever.

## Data sources

| | Windows | Linux | macOS |
|---|---|---|---|
| GPU list | `D3DKMT` via `gdi32` | `/sys/class/drm` | IOKit |
| GPU metrics | NVML | NVML | none |
| GPU match | PCI bus id | PCI bus id | none |
| CPU clock | `CallNtPowerInformation` | `/proc/cpuinfo` | `hw.cpufrequency` |
| CPU temp | perflib thermal zone | `/sys/class/thermal` | AppleSMC |
| RAM speed, CAS | WMI | `dmidecode` | none |
| Kernel | `RtlGetVersion` + registry `UBR` | `uname` | `uname` |
| Display mode | `QueryDisplayConfig` | DRM `modes` | CoreGraphics |
| Monitor name, size | EDID in the registry | EDID in sysfs | EDID in the IOKit display service |
| Host model | DMI in the registry | `/sys/class/dmi/id` | `hw.model` |
| Battery | none | none | IOKit `AppleSmartBattery` |
| Uptime | `GetTickCount64` | `/proc/uptime` | `kern.boottime` |

## Gotchas

- The perflib entry points are exported by **advapi32**, not ntdll, despite
  what the documentation implies. `EnumSystemFirmwareTables` and
  `SystemFirmwareTableInformation` both return empty on modern Windows, so
  raw SMBIOS is not an option there.
- WMI `Win32_PhysicalMemory` rejects a named projection with
  `WBEM_E_INVALID_QUERY`. Use `SELECT *` and read properties by name.
  `ConfiguredClockSpeed` is `REG_SZ`-style text, not binary. Most firmware
  does not report CAS latency at all.
- `Registry CurrentBuildNumber` is a string. Reading it as a DWORD yields 0.
- On POSIX, a blocking `connect` hangs for the kernel default. Use a
  non-blocking socket plus `poll`. `getaddrinfo` has no timeout, so the
  whole lookup belongs on its own thread with a bounded join.
- Output over ssh needs `\r\n`. A bare LF moves down without resetting the
  column and consecutive lines merge. Windows text mode already translates,
  so only POSIX needs the explicit CR.
- POSIX reads `/proc/self/exe` to find the disk volume. Strip exactly one
  path component, and keep the field far wider than `C:\` needs.
- NVML is deliberately never shut down or unloaded. The process exits
  immediately afterwards and both calls cost milliseconds per `--live` tick.
- A coloured load needs 25 bytes: colour code, percentage, the word and the
  reset. A 16 byte buffer cut the text off from 50 percent load upwards.
- The refresh rate is a fraction of a hertz on every platform: the Windows
  rational is 60000/1000 for 60 Hz, the Linux mode string is read as 60/1.
- `IOServiceGetMatchingService` consumes the reference of the matching
  dictionary. Releasing it afterwards is a double free and kills the
  process before it prints anything.
- `CGDisplayScreenSize` returns a `CGSize` in millimetres, one value per
  side, not a diagonal.
- `pthread_timedjoin_np` is a GNU extension and does not exist on macOS.
  The bounded wait uses a mutex with a condition variable instead.
- `_POSIX_C_SOURCE` alone hides the BSD types the macOS system headers use
  themselves. macOS needs `_DARWIN_C_SOURCE` as well.
- `CGDisplayModeGetRefreshRate` returns 0 for displays with a variable
  refresh rate, and `CGDisplayScreenSize` returns 0 for displays that do
  not report a physical size. Both mean the field is unknown, not zero.

## Layout

`src/main.c` is the entry point on every platform. Collectors live in
`src/*.c`, the Windows WMI and COM parts in `src/*.cpp`, the POSIX collectors
in `src/posix/`. `src/gpu_nvidia.c` is dead and in no target.

## Docs

`plan.md` is the design document, `progress.html` the session log,
`release-notes-3.5.md` the published notes. `README.md` for users.
