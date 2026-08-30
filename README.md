# hardfetch

`neofetch` for hardware — Windows / WSL2 / macOS. C11/C++17, MSVC `/MT`, no deps.

## Install

Prebuilt in `bin/`:
- Windows: `bin/hardfetch.exe`
- Linux/WSL2: `bin/hardfetch`
- macOS arm64: `bin/hardfetch-darwin-arm64`

Alias `hrf`:
```powershell
# Windows
Set-Alias hrf C:\path\to\bin\hardfetch.exe
# WSL/Linux/macOS
alias hrf=~/path/to/hardfetch
```

## Usage

```
hardfetch          # system info
hardfetch -n       # + network (interfaces + public IP)
hardfetch -d       # SMC dump (macOS)
hardfetch -v/-h    # version / help
```

## Build

```powershell
# Windows (VS 2022 x64)
cmake -B build -A x64
cmake --build build --config Release
```
```bash
# Linux / WSL2 / macOS
cmake -B build -S .
cmake --build build
```

## Layout

`src/main.c` → `src/*.c` (Win) + `src/posix/*.c` (Apple/Linux) + `src/*.cpp` (WMI/COM).

Docs: `plan.md` (spec), `AGENTS.md` (quirks), `progress.html` (log).
