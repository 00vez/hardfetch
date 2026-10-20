# hardfetch

![release](https://img.shields.io/github/v/release/00vez/hardfetch?style=flat-square)
![license](https://img.shields.io/github/license/00vez/hardfetch?style=flat-square)
![macos](https://img.shields.io/github/actions/workflow/status/00vez/hardfetch/macos.yml?label=macos&style=flat-square)

`neofetch` for hardware. One static binary, about 220 KB, about 50 ms from
process start. No config file, no daemon, nothing to download.

```
hardfetch v3.5 | user@DESKTOP-EXAMPLE

OS           Windows 11 Pro 25H2 | x86_64
Host         X670E EXAMPLE BOARD WIFI
Uptime       11h 57m

CPU          AMD Ryzen 7 5800X
             4.19 GHz | 23% Load | 48 °C

RAM          13.42 / 31.12 GiB | 43%
             3200 MHz | CL16

GPU          NVIDIA GeForce RTX 3070
             14% Load | 61 °C | 187 W
             VRAM 3.21 / 8.00 GiB | 40%

Disk         812.44 / 931.32 GiB | 87%
IP (WLAN)    192.168.178.36/24
```

## Download

```powershell
# Windows
iwr https://github.com/00vez/hardfetch/releases/latest/download/hardfetch.exe -OutFile hardfetch.exe
```
```bash
# Linux, WSL2, macOS und Git Bash unter Windows
os=$(uname -s) arch=$(uname -m); case "$os-$arch" in Linux-x86_64) n=hardfetch ;; Darwin-arm64) n=hardfetch-macos-arm64 ;; Darwin-x86_64) n=hardfetch-macos-x64 ;; MINGW*|MSYS*|CYGWIN*) n=hardfetch.exe ;; *) echo "hardfetch: no binary for $os $arch" >&2; false ;; esac && curl -fsSL "https://github.com/00vez/hardfetch/releases/latest/download/$n" -o hardfetch && chmod +x hardfetch
```
`latest` points at the newest release. The macOS line covers both Apple
silicon and Intel.

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

`q`, `Esc` or `Ctrl+C` quits the live view. With redirected output it
prints one frame and exits.

## Platforms

Windows, Linux/WSL2 and macOS produce the same layout. A field the
platform cannot supply is left out rather than guessed.

| | Windows | Linux | macOS |
|---|---|---|---|
| display | yes | yes | yes |
| CPU temperature | yes | yes | yes |
| RAM speed | yes | yes | no |
| iGPU details | yes | yes | yes |
| battery | yes | yes | yes |

## Todo

- Linux ARM binary
- Public IP on Linux
- Battery temperature on Windows and Linux
- Battery cycle count on Windows, from `powercfg /batteryreport /xml`

## Build

```bash
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

## License

MIT
