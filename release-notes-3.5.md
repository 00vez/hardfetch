macOS is in. Three platforms, one output format.

| | `hardfetch.exe` | `hardfetch` | `hardfetch-macos-arm64` | `hardfetch-macos-x64` |
|---|---|---|---|---|
| platform | Windows x64 | Linux / WSL2 x64 | macOS Apple silicon | macOS Intel |
| self-reports | `v3.5` | `v3.5` | `v3.5` | `v3.5` |
| flags | `-a -l -d -v -h` | same | same | same |
| layout | shared renderer | same renderer | same renderer | same renderer |
| startup | about 50 ms | about 20 ms | TBD | TBD |

All four are built from the same tree and print through the same
`render.c`, `fmt.c`, `edid.c`, `output.c` and `debug.c`. That is what
makes the output identical rather than merely similar. Only what each
platform can read differs:

| | Windows | Linux | macOS |
|---|---|---|---|
| display mode | QueryDisplayConfig | `/sys/class/drm/*/modes` | CoreGraphics |
| monitor name and size | EDID in the registry | EDID in `/sys/class/drm/*/edid` | EDID in the IOKit display service |
| CPU temperature | perflib thermal zone | `/sys/class/thermal` | AppleSMC |
| RAM speed and CAS | WMI | dmidecode | none |
| best interface | interface type scoring | `/proc/net/route` | interface type scoring |
| GPU | D3DKMT plus NVML | sysfs plus NVML | IOKit |
| battery | none | none | IOKit `AppleSmartBattery` |

A field the platform cannot supply is left out rather than guessed. On WSL
the Linux view is shorter: no thermal zone, no DRM card and no DMI table,
so the CPU temperature, the second GPU and the display row are absent
there. All three are implemented and were checked against fixture sysfs
trees, so they appear on an ordinary Linux box.

## macOS

The Apple branches had been written for the v0.2.4 collector shape and
had not been compiled since August 2025. Three things were missing.

Display had no path at all. `display_posix.c` only knew the DRM sysfs
tree, which macOS does not have. CoreGraphics supplies the active
displays, their mode and their refresh rate, the physical size comes
from `CGDisplayScreenSize`, and the monitor name from the EDID in the
IOKit display service. With more than one display the services cannot
be matched to the screens without guessing, so the name is left out
then instead of being invented.

Host read the model from DMI, which macOS does not have either. It now
comes from `hw.model`, the same identifier the system information shows.

Battery was dead code. It printed through `print_block`, the v0.2.4
output path, was attached to no state and was called from nowhere. It
now fills `hf_state` and the renderer draws the row, from the IOKit
`AppleSmartBattery` service: charge, charging state, temperature and
cycle count. A Mac without a battery has no such service, and then the
row is left out.

The binaries are built and run on a real Mac by a workflow, and the
output of the runner is kept as the evidence. The Apple silicon binary
was executed on the runner; the Intel one is cross compiled from the
same SDK and runs under Rosetta where that is available.

Two more things had to change for macOS to build at all. The bounded
wait for a thread used `pthread_timedjoin_np`, which is a GNU
extension and does not exist there; it now uses a mutex with a
condition variable, which works on every platform. And `_POSIX_C_SOURCE`
on its own hides the BSD types the macOS system headers use themselves,
so the Darwin namespace is asked for as well.

## Fixed in 3.5

The macOS run on the runner made visible what the other platforms were
getting wrong as well.

- The load field had a buffer of 16 bytes. A coloured load needs the
  colour code, the percentage, the word `Load` and the reset, which is 25
  bytes. From 50 percent load upwards the text was cut off, so the row
  read `64%` without the word and without the reset. Checked with 70
  percent load.
- The refresh rate was multiplied by a thousand. Both collectors store
  the rate as a fraction of a hertz, the Windows rational is 60000/1000
  for 60 Hz and the Linux mode string is read as 60/1, so the division
  was enough. A 240 Hz monitor showed 240000 Hz. Checked against the DRM
  fixture, it now reads 240 Hz as the plan wants it.
- An integrated adapter was only shown in the full view, so a machine
  without a discrete adapter showed no GPU at all. That is the case on
  every Apple silicon Mac, where the Apple GPU is the only adapter.
  When there is no discrete adapter, the integrated one is now shown
  like one. Machines with a discrete adapter are unaffected.
- The OS line on macOS read `macOS 14.8.9 23`, where 23 is the major
  version of Darwin and not a version of macOS. The product already
  carries the marketing version, so the second field stays empty and the
  Darwin release moves to the build field.
- `IOServiceGetMatchingService` consumes the reference of the matching
  dictionary. Releasing it afterwards freed the object a second time
  and the process died before it printed anything.

## What changed in 3.0 to 3.4

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
- The public IP lookup runs on its own thread with bounded resolution,
  connect and a deadline before the first frame. On a VM whose resolver
  intermittently stalls five seconds, `-a` used to take 5.2 s before
  showing anything; it is now capped, measured at 166 ms worst case over
  25 runs.
- `disk_volume` was eight bytes, so `/home/user` truncated to `/home/u`,
  statvfs failed and the Disk row disappeared on Linux.
- Output over ssh used a bare LF. A terminal moves down without resetting
  the column, so consecutive lines merged once a line passed column 78.

## Flags

- `-a`  standard view plus build, kernel, shell, terminal, iGPU, display,
       every interface and the public IP
- `-l [seconds]`  refresh in place, q, Esc or Ctrl+C to quit
- `-d`  per module timings, chosen data source and failures on stderr

`--net` and `-n` are gone on all platforms.

## Verify

```
sha256sum -c SHA256SUMS
```

## Known gaps

- CPU CAS latency usually shows nothing on Windows. Most firmware does not
  expose it and raw SMBIOS has been closed to user mode since Windows 10
  1607.
- Swap is shown on no platform. Linux reads SwapTotal and SwapFree
  from /proc/meminfo, but Win32 only offers the commit limit, not the
  used page file, so a row would not mean the same thing on all of them.
- The battery row is filled on macOS only. Windows laptops have a
  battery, but there is no collector for it yet, and Linux has none
  either.
- The CPU clock on macOS is the nominal clock from `hw.cpufrequency`. A
  live clock would need the voltage states of the processor, which only
  Intel Macs publish. Where the kernel does not answer at all, the row
  shows N/A.
- The monitor name on macOS needs the EDID of the display service. With
  more than one display the name is left out rather than assigned to the
  wrong screen.
- The Intel binary is compiled from the same SDK as the Apple silicon one
  and runs under Rosetta where that is installed. It was not run on a
  physical Intel Mac.
- `-l` needs a real terminal. With redirected output all binaries print
  one frame and exit rather than looping forever, so a pipe never fills up
  with an endless redraw.
