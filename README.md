# hardfetch

`neofetch` for hardware. Windows, C11/C++17, MSVC `/MT`, statically linked, 212 KB.

The standard view takes about 28 ms from process start. No config file, no
daemon, no assets to download.

```
hardfetch v3.3 | user@DESKTOP-EXAMPLE

OS           Windows 11 Pro 25H2 | x86_64
Build        26200.5674
Kernel       WIN32_NT 10.0.26200.5674
Host         X670E EXAMPLE BOARD WIFI
Shell        PowerShell 7.6.2
Terminal     Windows Terminal | 120x34 | truecolor
             Cascadia Mono 11x22
Uptime       11h 57m

CPU          AMD Ryzen 7 5800X
             4.19 GHz | 23% Load | 48 °C
             boost 4.90 GHz | 8C/16T

RAM          13.42 / 31.12 GiB | 43%
             3200 MHz | CL16
             DDR4 | 32.00 GiB

GPU          NVIDIA GeForce RTX 3070
             14% Load | 61 °C | 187 W
             VRAM 3.21 / 8.00 GiB | 40%
             Core 2010 MHz | Memory 7001 MHz

GPU 2        AMD Radeon(TM) Graphics
             VRAM 512.00 MiB

Disk         812.44 / 931.32 GiB | 87%
IP (WLAN)    192.168.178.36/24
IP (Public)  203.0.113.42
Ethernet 2  192.168.137.1/24
Tailscale  100.88.253.13/32
```

## Install

Prebuilt in `bin/`:
- Windows: `bin/hardfetch.exe`
- Linux / WSL2: `bin/hardfetch`

Build from source needs Visual Studio 2022 x64 and CMake 3.20:

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

## Usage

```
hardfetch            # standard view
hardfetch -a         # plus build, kernel, shell, terminal, iGPU,
                     # display, every interface and the public IP
hardfetch -l         # refresh in place, every second
hardfetch -l 5       # refresh in place, every five seconds
hardfetch -d         # per module timings, chosen source and failures
hardfetch -v/-h      # version / help
```

In `--live` mode `q`, `Esc` or `Ctrl+C` quits. `q` is only read from a
console; with redirected output the frame is printed once and the process
exits.

`--smc-dump` still exists for macOS and has no short form, because `-d`
is `--debug` now. `--net` and `-n` were removed; `-a` covers that ground.

## Platform support

Windows and Linux/WSL2 produce the same layout and the same flags. What
differs is only what each platform can read:

| | Windows | Linux |
|---|---|---|
| display mode | QueryDisplayConfig | `/sys/class/drm/*/modes` |
| monitor name and size | EDID in the registry | EDID in `/sys/class/drm/*/edid` |
| CPU temperature | perflib thermal zone provider | `/sys/class/thermal` |
| RAM speed and CAS | WMI | dmidecode, needs privileges |
| GPU | D3DKMT plus NVML | sysfs plus NVML |

A field the platform cannot supply is left out rather than guessed. On WSL
the Linux view is shorter than the Windows one on the same machine: no
thermal zone, no DRM connector and no `/proc/net/thermal_zone*`, so the CPU
temperature, the second GPU and the display row are absent there. All three
are implemented and were checked against fixture sysfs trees.

macOS is not covered by this release.

### Package managers

```powershell
# scoop, from the manifest in this repository
scoop install https://raw.githubusercontent.com/00vez/hardfetch/master/packaging/scoop/hardfetch.json
```
```bash
# homebrew, from a tap that hosts the formula in packaging/brew
brew install 00vez/hardfetch/hardfetch
```

The winget manifest is in `packaging/winget/00vez.hardfetch.yaml`. Putting it
in a package source still needs an upstream pull request, which is not done.

## Runtime dependencies

The executable itself has none, but it talks to services the operating
system already provides: WMI for the RAM speed and CAS latency, the
perflib provider in `advapi32` for the CPU temperature, D3DKMT in `gdi32`
for the GPU list, and `nvml.dll` from the NVIDIA driver if one is present.
Everything degrades to a missing field rather than an error.

CPU CAS latency is usually not shown: most firmware does not expose it,
and raw SMBIOS tables have been closed to user mode since Windows 10 1607.

## Layout

`src/main.c` is the entry point on both platforms. Collectors live in
`src/*.c`, the Windows WMI and COM parts in `src/*.cpp`, and the POSIX
collectors in `src/posix/`. `render.c`, `fmt.c`, `edid.c`, `output.c` and
`debug.c` are shared by both, which is what keeps the output identical.

Docs: `AGENTS.md` (build and quirks), `plan.md` (spec), `progress.html`
(log).

## License

See `LICENSE`.
