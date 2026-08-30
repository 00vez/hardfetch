# hardfetch — agent instructions

Cross-platform `neofetch`-like tool (C11/C++17, MSVC `/MT`, zero deps).

## Build & run

```powershell
# Windows (needs VS 2022 x64)
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\hardfetch.exe [--net|--version|--help]
```
```bash
# Linux / WSL2 / macOS (AppleClang/gcc)
cmake -B build -S .
cmake --build build
./build/hardfetch [--net|--version|--help]
```

`build-wsl` / `build-silicon` separate dirs for cross checks.

## Architecture

Single target, `src/main.c` entry. C11 `.c`, C++17 for `cpu_temp.cpp`, `memory_spd.cpp`, `gpu.cpp`. `extern "C"` in shared headers (`output.h`, `cpu_temp.h`). CMake: `LINKER_LANGUAGE C`, empty `CMAKE_CXX_IMPLICIT_LINK_LIBRARIES`.

POSIX: `src/posix/*.c` — `apple_pmgr.c` (pmgr `voltage-states*`), `apple_smc.c` (SMC `IOConnectCallStructMethod`), `battery_posix.c`.

## Output

- ANSI: labels gray (90), values white/green, bold gray sections
- No logo/frame, single pass, 2-space indent, K&R
- `SetConsoleOutputCP(CP_UTF8)` + `ENABLE_VIRTUAL_TERMINAL_PROCESSING` (Win)

## Quirks

- `cpu_temp.cpp` / `memory_spd.cpp` / `gpu.cpp` — WMI/COM (C++), NVML via `LoadLibraryA("nvml.dll")`
- `gpu_nvidia.c` dead — replaced by `gpu.cpp`
- `storage.c` — `GetDiskFreeSpaceExA` (%+GiB), no temp
- `cpu.c` — `GetSystemTimes` load; `cpu_posix.c` — `host_processor_info` (macOS 200ms) / `/proc/stat` (Linux)
- `os_info.c` — `CurrentBuildNumber >=22000` → Windows 11
- `shell.c` — registry `PowerShellVersion`, fallback file version
- macOS: `HOST_NAME_MAX` → `MAXHOSTNAMELEN`, `u_int` typedefs before `sys/sysctl.h`, `mach_host_t` → `mach_port_t`, `IFF_UP` fallback

## Known

CPU base not boost on Ryzen (CPPC, PDH 100%); AMD iGPU VRAM 0; Temp N/A where no sensor.

See `plan.md` for spec, `progress.html` for log.
