# hardfetch — agent instructions

Cross-platform `neofetch`-style tool. Windows is the tested target; Linux and
WSL2 share the same output. C11 for `.c`, C++17 only where WMI/COM is needed
(`memory_spd.cpp`, `cpu_temp.cpp`, `gpu.cpp`). MSVC `/MT`, statically
linked, 219 KB, no external runtime dependencies.

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

`-DHF_ASAN=ON` builds a sanitiser variant. Its DLL sits next to the MSVC
tools; it is not on PATH by default.

**Always build clean before signing off.** Incremental builds reuse object
files, and a stale `output.h` or `hf_state.h` hides warnings and layout
mismatches. Two warnings and one shared-header breakage only appeared with
`--clean-first`.

## Output

One renderer for both platforms: `render.c`, `fmt.c`, `edid.c`, `output.c`,
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

| | Windows | Linux |
|---|---|---|
| GPU list | `D3DKMT` via `gdi32` | `/sys/class/drm` |
| GPU metrics | NVML | NVML |
| GPU match | PCI bus id | PCI bus id |
| CPU clock | `CallNtPowerInformation` | `/proc/cpuinfo` |
| CPU temp | perflib thermal zone | `/sys/class/thermal` |
| RAM speed, CAS | WMI | `dmidecode` |
| Kernel | `RtlGetVersion` + registry `UBR` | `uname` |
| Display mode | `QueryDisplayConfig` | DRM `modes` |
| Monitor name, size | EDID in the registry | EDID in sysfs |
| Uptime | `GetTickCount64` | `/proc/uptime` |

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

## Layout

`src/main.c` is the entry point on both platforms. Collectors live in
`src/*.c`, the Windows WMI and COM parts in `src/*.cpp`, the POSIX collectors
in `src/posix/`. `src/gpu_nvidia.c` is dead and in no target.

## Docs

`plan.md` is the design document, `progress.html` the session log,
`release-notes-3.4.md` the published notes. `README.md` for users.
