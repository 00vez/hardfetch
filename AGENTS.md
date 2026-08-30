# hardfetch — agent instructions

Windows-only `neofetch`-like system info tool (C11/C++17, MSVC `/MT`, zero external deps).

## Build & run

```powershell
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\hardfetch.exe [--net|--version|--help]
```

Requires **VS 2022 x64 developer prompt** (or `-A x64`). Generator is Visual Studio 17 2022 (MSBuild). No Ninja.

## Architecture

Single CMake target, flat `src/`. Entry: `src/main.c`. C11 for `.c` files; C++17 for WMI/COM files (`cpu_temp.cpp`, `memory_spd.cpp`, `gpu.cpp`). Headers shared with `.cpp` files use `extern "C"` blocks (`output.h`, `cpu_temp.h`, `gpu.h`, `memory_spd.h`). Pure-C-only headers (`cpu.h`, `host.h`, `memory.h`) don't need them.

CMake quirks: `LINKER_LANGUAGE C` override on mixed target, empty `CMAKE_CXX_IMPLICIT_LINK_LIBRARIES` to suppress libstdc++.

## Runtime requirements

- Admin rights needed: ICMP ping (`--net`), NVMe temp, CPU power, WMI thermal zones (CPU temp)
- Network section hidden unless `--net` flag passed
- No VC++ Redistributable needed (static `/MT`)

## Output quirks

- ANSI colors: labels gray, values white/green, sections bold gray
- No logo or frame
- Single pass, no loops or sleeps
- 2-space indent, K&R braces in source

## Module quirks

- `cpu_temp.cpp` and `memory_spd.cpp` use WMI COM (C++), not pure C
- `gpu.cpp` (not `gpu_nvidia.c`) handles NVIDIA via dynamic NVML + WMI
- `storage.c` uses `GetDiskFreeSpaceExA` (usage % + GB/TiB), not temp by default
- `network.c` is async thread + 4 ICMP pings (avg + jitter) + WinHTTP throughput tests
- `cpu.c` uses `GetSystemTimes()` for load, not PDH
- `os_info.c` replaces "Windows 10" with "Windows 11" for build >= 22000
- `shell.c` reads PowerShell version from registry, not EXE file version

## Gotchas an agent will miss

- **Dead file**: `src/gpu_nvidia.c` is **not compiled** — `gpu.cpp` replaced it (WMI multi-GPU + NVML dynamic)
- **Degree symbol**: MSVC can't parse `"\xc2\xb0"` inside a string literal — use `"\xc2\xb0""C"` (concatenation) everywhere
- **CPU name stripping**: `cpu.c:78-84` uses digit-only skip; model number preserved
- **IP selection**: priority-based in `ip_addr.c` (WLAN > Ethernet > tunnel/other)
- **Shell version**: `shell.c` reads `PowerShellVersion` from Registry; for Store-installed PS7 (pwsh.exe), falls back to file version
- **CPU freq**: PDH `Processor Performance(PPM_Processor_0)\% of Maximum Frequency` — returns 100% on AMD Ryzen (CPPC), boost unmeasurable in user mode
- **CPU load**: `GetSystemTimes()` difference, not PDH
- **Storage**: `GetDiskFreeSpaceExA` for usage (GiB/TiB + %); no more temp
- **Network**: async thread → 4 ICMP pings (avg + jitter), `GetIfEntry2` for link speed; admin needed for ping
- **Output init**: `SetConsoleOutputCP(CP_UTF8)` + `ENABLE_VIRTUAL_TERMINAL_PROCESSING`
- **Win 11 detection**: `os_info.c` checks `CurrentBuildNumber >= 22000` → replaces "Windows 10" with "Windows 11" in ProductName

## GPU

NVML loaded dynamically via `LoadLibraryA("nvml.dll")` inside `gpu.cpp` — never link statically. If NVIDIA driver absent, prints `[NVIDIA driver not found]`. Minimal `include/nvml.h` exists (used as reference; gpu.cpp also has inline typedefs).

## Known issues (not yet fixed)

| Issue | Root cause |
|-------|-----------|
| CPU shows base (4.2) not boost (5.05 GHz) | PDH `% of Maximum Frequency` returns 100% on AMD Ryzen (CPPC); boost not measurable in user mode |
| AMD iGPU VRAM 0, no clock | WMI AdapterRAM returns 0; needs DXGI |
| Temp N/A on some Ryzen | k10temp not accessible via WMI |

## Fixed issues (2026-06-11)

| Fix | What changed |
|-----|-------------|
| CPU name stripping | `cpu.c`: changed while loop from `!= ' '` to digit-only skip, preserving model number |
| CPU boost attempt | `cpu.c`: corrected PDH path to `Processor Performance(PPM_Processor_0)\% of Maximum Frequency` |
| IP preferring WLAN | `ip_addr.c`: priority-based adapter selection (WLAN > Ethernet > tunnel/other) |
| Shell engine version | `shell.c`: reads `PowerShellVersion` from Registry instead of EXE file version |
| PS7 Store install detection | `shell.c`: uses `QueryFullProcessImageNameW` for full path + file version fallback |
| Win 11 detection | `os_info.c`: `CurrentBuildNumber >= 22000` → replaces "Windows 10" with "Windows 11" |
| Host name cleaned | `host.c`: removed manufacturer prefix, shows only `BaseBoardProduct` |
| Disk usage instead of temp | `storage.c`: `GetDiskFreeSpaceExA` for GiB/TiB + %, removed NVMe temp entirely |
| user@hostname header | `main.c`: `GetUserNameA` + `GetComputerNameA` with separator |
| Network jitter + link speed | `network.c`: 4-ping avg + jitter, `GetIfEntry2` for link speed |

## Design docs

`plan.md` (German) is the authoritative design doc with per-module specs and verification commands. Consult before adding features.