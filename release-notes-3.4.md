Two binaries, one output format.

| | `hardfetch.exe` | `hardfetch` |
|---|---|---|
| platform | Windows x64 | Linux / WSL2 x64 |
| self-reports | `v3.4` | `v3.4` |
| flags | `-a -l -d -v -h` | same |
| layout | shared renderer | same renderer |
| startup | about 50 ms | about 20 ms |

Both are built from the same tree and print through the same `render.c`,
`fmt.c`, `edid.c`, `output.c` and `debug.c`. That is what makes the output
identical rather than merely similar. Only what each platform can read
differs:

| | Windows | Linux |
|---|---|---|
| display mode | QueryDisplayConfig | `/sys/class/drm/*/modes` |
| monitor name and size | EDID in the registry | EDID in `/sys/class/drm/*/edid` |
| CPU temperature | perflib thermal zone | `/sys/class/thermal` |
| RAM speed and CAS | WMI | dmidecode |
| best interface | interface type scoring | `/proc/net/route` |
| GPU | D3DKMT plus NVML | sysfs plus NVML |

A field the platform cannot supply is left out rather than guessed. On WSL
the Linux view is shorter: no thermal zone, no DRM card and no DMI table,
so the CPU temperature, the second GPU and the display row are absent
there. All three are implemented and were checked against fixture sysfs
trees, so they appear on an ordinary Linux box.

## Windows

The standard view takes about 50 ms from process start, against 283 ms for
v0.2.4. fastfetch needs 17 ms for a larger report; the executable is
219 KB against its 11.5 MB.

```
hardfetch v3.4 | user@DESKTOP-EXAMPLE

OS           Windows 11 Pro 25H2 | x86_64
Build        26200.5674
Kernel       WIN32_NT 10.0.26200.5674
Host         X670E EXAMPLE BOARD WIFI
Shell        PowerShell 7.6.2
Terminal     Windows Terminal | 120x34 | truecolor
Uptime       11h 57m

CPU          AMD Ryzen 7 5800X
             4.19 GHz | 23% Load | 48 °C
             8C/16T | boost 4.90 GHz

RAM          13.42 / 31.12 GiB | 43%
             6000 MHz | CL16
             DDR5 | 32.00 GiB

GPU          NVIDIA GeForce RTX 3070
             14% Load | 61 °C | 187 W
             VRAM 3.21 / 8.00 GiB | 40%
             Core 2010 MHz | Memory 7001 MHz

Disk         812.44 / 931.32 GiB | 87%
Display      1920x1080 | 240 Hz | 32"
             32G2WG8
IP (WLAN)    192.168.178.36/24
IP (Public)  203.0.113.42
```

## Fixed in 3.4

- The public IP lookup runs on its own thread with bounded resolution,
  connect and a deadline before the first frame. On a VM whose resolver
  intermittently stalls five seconds, `-a` used to take 5.2 s before
  showing anything; it is now capped, measured at 166 ms worst case over
  25 runs.
- `disk_volume` was eight bytes, so `/home/user` truncated to `/home/u`,
  statvfs failed and the Disk row disappeared on Linux.
- Output over ssh used a bare LF. A terminal moves down without resetting
  the column, so consecutive lines merged once a line passed column 78.
- `/proc/cpuinfo` puts the clock in the model name, so the CPU row read
  "i3-7100T CPU @ 3.40GHz".
- Two threads wrote to stderr unlocked and cut each other's lines in half.
- PCI vendor 1B36 was labelled QEMU; it is Red Hat's QXL display.
- CPU load flipped between 0 and 100 percent because a 20 ms window on a
  two thread VM is four jiffies.
- `-a` now shows the distribution point release and codename, from
  VERSION rather than the major-only VERSION_ID.

## What changed in 3.0 to 3.3

- Output rewritten: label column at 14, one line per monitor and per network
  interface, severity colours for load, temperature and VRAM fill.
- Fixed the AMD iGPU reporting 0 VRAM. It now reports the real dedicated
  segment size, on both platforms.
- CPU temperature resolves instead of always showing N/A.
- Kernel version from RtlGetVersion, build number read from the registry as
  a string, disk volume detected from the executable path.
- Display works. The display configuration API needs an interactive
  session and refuses otherwise, which fastfetch hits too; a registry
  EDID fallback runs when it does.

## Data sources

- GPU enumeration via D3DKMT instead of WMI, which is what caused the iGPU
  bug and cost 21 ms per run.
- NVML devices matched by PCI bus id instead of by name substring.
- CPU clock via CallNtPowerInformation; the PDH counter needs a two-sample
  window whose first collect costs 180 ms cold.
- CPU temperature via the perflib provider in advapi32, which is exported
  by advapi32 and not by ntdll despite what the documentation implies.

## Flags

- `-a`  standard view plus build, kernel, shell, terminal, iGPU, display,
       every interface and the public IP
- `-l [seconds]`  refresh in place, q, Esc or Ctrl+C to quit
- `-d`  per module timings, chosen data source and failures on stderr

`--net` and `-n` are gone on both platforms.

## Verify

```
sha256sum -c SHA256SUMS
```

## Known gaps

- CPU CAS latency usually shows nothing on Windows. Most firmware does not
  expose it and raw SMBIOS has been closed to user mode since Windows 10
  1607.
- Swap is shown on neither platform. Linux reads SwapTotal and SwapFree
  from /proc/meminfo, but Win32 only offers the commit limit, not the
  used page file, so a row would not mean the same thing on both.
- macOS is not in this release. The Apple branches compile against the
  old collector shape and were not retested; there is no Apple toolchain
  in the build environment.
- `-l` needs a real terminal. With redirected output both binaries print
  one frame and exit rather than looping forever, so a pipe never fills up
  with an endless redraw.
