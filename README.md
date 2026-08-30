# hardfetch

Compact `neofetch`-like system info for Windows (C11/C++17, MSVC `/MT`, zero external dependencies). 
Also runs in WSL2/Linux (POSIX) and builds for macOS (`__APPLE__` branches in `src/posix/*.c`).

## Quick install (prebuilt binaries — copy once)

The `setup.ps1` script in this repo can refresh / install on a new device:
```powershell
# In Windows PowerShell (repo root, or copy setup.ps1 + bin/ first)
.\tools\setup.ps1
```
This will:
- Set PowerShell alias `hrf` → `bin\hardfetch.exe` (or existing binary path)
- Copy Linux binary from repo `bin/hardfetch` to `~/.local/bin/hardfetch` (WSL2) and add zsh `hrf()` alias

If you prefer manual / first setup without script (or after `git clone` without build):

```powershell
# Windows — copy built binary to your PATH, then alias
mkdir -Force $env:USERPROFILE\bin
Copy-Item build\Release\hardfetch.exe $env:USERPROFILE\bin\hardfetch.exe
Set-Alias hrf $env:USERPROFILE\bin\hardfetch.exe  # or in $PROFILE
```

```bash
# WSL2 / Linux — after building or copying bin/hardfetch
mkdir -p ~/.local/bin
cp /mnt/c/Users/v/Documents/lib/hardfetch/bin/hardfetch ~/.local/bin/hardfetch
chmod +x ~/.local/bin/hardfetch
# Add alias in ~/.zshrc (or distribute in this repo's setup-wsl.sh)
echo 'alias hrf="~/.local/bin/hardfetch"' >> ~/.zshrc
```

## Usage

```bash
./hardfetch           # default sections
./hardfetch -n --net  # show network interfaces + public IP
./hardfetch -v        # version
./hardfetch -h        # help
```

Note: `-n` / `--net` is hidden by default; network lookup requires internet access.

## Building

**Windows (MSVC x64, requires VS Build Tools):**
```powershell
cmake -B build -A x64
cmake --build build --config Release
```

**WSL2 / Linux (glibc):**
```bash
cmake -S . -B build-wsl
cmake --build build-wsl
./build-wsl/hardfetch
```

## Project
- Entry: `src/main.c`
- Windows modules: `src/*.c`, `src/*.cpp` (WMI/COM for GPU, thermal, SPD)
- POSIX (Linux/macOS): `src/posix/*.c`
- Design doc: `plan.md`

## License / Author
Own tool; see `AGENTS.md` for build notes and known issues.
