# hardfetch — Implementierungsplan (realisiert v0.2.4, Stand 31.08.2026)

> Original 0.1-Plan (Windows-only) unten unverändert — darüber IST-Stand: POSIX-Port via `src/posix/`, `apple_pmgr`/`apple_smc`, WSL/macOS verifiziert. Für Historie `progress.html`.

## Projektprinzipien (Non-Negotiable)

| Prinzip | Umsetzung |
|---------|-----------|
| **Neubau** | Kein Fastfetch-Fork, reiner Eigenbau. `fork/` nur als Referenz. |
| **Sprache** | C11 für Core + Module, **C++ nur für WMI/COM Fallback** (`*.cpp`) |
| **Plattform** | Windows 10/11 x64 + WSL2/Linux + macOS arm64 (realisiert v0.2.4) |
| **Portabilität** | MSVC **/MT** statisch linken, keine VC++ Redist nötig |
| **GPU** | NVIDIA only via **dynamisches Laden** von `nvml.dll` |
| **Admin** | OK für WMI/SPD/NVMe Temp — Tool zeigt was geht, sonst `N/A` |
| **Storage** | 500 ms Messintervall, **0 MB/s → Zeile auslassen** |
| **Output** | Kompakt, **Labels Grau / Werte Grün-Weiß**, kein Logo, kein Rahmen |
| **Flags** | `--net` (Netzwerk an/aus), `--version`, `--help` |

---

## Ziel-Output

```
hardfetch v0.1

  OS       Windows 11 Pro 26100

  CPU      AMD Ryzen 9 7950X @ 5.7 GHz
           Load  34%  |  Temp  71°C  |  Power  88 W

  GPU      NVIDIA GeForce RTX 4090
           Core  2520 MHz  |  Mem  10501 MHz  |  Load  67%
           VRAM  18.4 / 24.0 GB  |  Temp  72°C  |  Power  320 W

  RAM      22.1 / 32.0 GB  |  6000 MHz  CL30

  Disk     Read  2450 MB/s  |  Write  1820 MB/s
           NVMe Temp  38°C
```

Mit `--net`:
```
  Net       12.4 MB/s  |   84.1 MB/s
```

---

## Dateistruktur

```
hardfetch/
  fork/                              ← Referenz (bestehend, nicht ändern)
  CMakeLists.txt
  README.md
  include/
    nvml.h                           ← NVIDIA SDK Header (kopieren)
  src/
    main.c                           ← Entry, Flags, Callgraph
    output.c / output.h              ← ANSI-Farbsystem + Print-Helfer
    os_info.c / os_info.h            ← Windows-Version + Build
    gpu_nvidia.c / gpu_nvidia.h      ← NVML dynamisch (C)
    cpu.c / cpu.h                    ← Name, Takt, Load (Registry + PDH, C)
    cpu_temp.c / cpu_temp.h          ← Temp, Power (Perflib C, WMI COM Fallback C++)
    memory.c / memory.h              ← Used/Total (GlobalMemoryStatusEx, C)
    memory_spd.c / memory_spd.h      ← Takt/Latenz (WMI COM, C++)
    storage.c / storage.h            ← I/O Speed (PDH) + NVMe Temp (DeviceIoControl, C)
    network.c / network.h            ← GetAdaptersAddresses (C)
```

---

## Schritt-für-Schritt-Plan

---

### Step 1 — Build-System & Hello World `[Must]`

**Ziel:** Kompilierbares Skelett mit CMake, statisch gelinkt, funktioniert auf jedem Windows-PC.

**Dateien:** `CMakeLists.txt`, `src/main.c`

**Konkrete Aufgaben:**

1. `CMakeLists.txt` anlegen:
   - `cmake_minimum_required(VERSION 3.20)`
   - `project(hardfetch VERSION 0.1 LANGUAGES C CXX)`
   - Windows-only Guard: `if(NOT WIN32) message(FATAL_ERROR "Windows only") endif()`
   - C11 für C-Dateien: `set(CMAKE_C_STANDARD 11)`
   - C++17 für C++-Dateien: `set(CMAKE_CXX_STANDARD 17)`
   - MSVC Flags: `/MT` (Release), `/MTd` (Debug), `/W4`
   - `/O2 /DNDEBUG` für Release
   - Source Files auflisten (initial nur `src/main.c`)
   - Suppress libstdc++ linkage: `set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")` und `set_target_properties(hardfetch PROPERTIES LINKER_LANGUAGE C)`
   - Windows System-Libs: `iphlpapi.lib pdh.lib wbemuuid.lib ole32.lib oleaut32.lib uuid.lib ntdll.lib`

2. `src/main.c` anlegen:
   - `#include <stdio.h>`, `#include <string.h>`
   - `int main(int argc, char* argv[])` mit Flag-Parsing
   - `--version` → `printf("hardfetch v0.1\n")`
   - `--help` → Usage-Text ausgeben
   - Sonst: `printf("hardfetch v0.1\n")` (Platzhalter)

**Verification:**
```powershell
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\hardfetch.exe --version
# Erwartet: hardfetch v0.1
.\build\Release\hardfetch.exe --help
# Erwartet: Usage-Text
```

---

### Step 2 — ANSI-Output-System `[Must]`

**Ziel:** Kompaktes Farblayout — Labels Grau, Werte Weiß/Grün, keine Dekoration.

**Dateien:** `src/output.c`, `src/output.h`

**Konkrete Aufgaben:**

1. `src/output.h`:
   ```c
   void output_init(void);               // VT-Processing aktivieren
   void print_label(const char* label);  // Grau: \x1b[90m
   void print_value(const char* value);  // Weiß: \x1b[97m
   void print_value_green(const char* v);// Grün: \x1b[92m
   void print_section(const char* title);// Fett + Grau
   void reset_color(void);               // \x1b[0m
   void print_newline(void);
   ```

2. `src/output.c`:
   - `output_init()`: `GetStdHandle(STD_OUTPUT_HANDLE)` → `SetConsoleMode` mit `ENABLE_VIRTUAL_TERMINAL_PROCESSING`
   - `print_label()`: `\x1b[90m` + Text + `\x1b[0m`
   - `print_value()`: `\x1b[97m` + Text + `\x1b[0m`
   - `print_value_green()`: `\x1b[92m` + Text + `\x1b[0m`
   - `print_section()`: `\x1b[1;90m` + Text + `\x1b[0m`

3. `src/main.c` anpassen: `output_init()` aufrufen, `--version` mit `print_value_green` ausgeben

**Verification:**
```powershell
.\build\Release\hardfetch.exe --version
# Erwartet: farbige Ausgabe "hardfetch v0.1"
```

---

### Step 3 — OS-Modul `[Must]`

**Ziel:** Windows-Version und Build-Nummer zuverlässig auslesen.

**Dateien:** `src/os_info.c`, `src/os_info.h`

**Konkrete Aufgaben:**

1. `src/os_info.h`:
   ```c
   void print_os_info(void);
   ```

2. `src/os_info.c`:
   - **Registry-Zugriff** (C, kein COM):
     - `RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key)`
     - `RegQueryValueExA(key, "ProductName", ...)  → "Windows 11 Pro"`
     - `RegQueryValueExA(key, "CurrentBuildNumber", ...)  → "26100"`
   - **Output:**
     ```
     print_section("OS");
     print_value("Windows 11 Pro 26100");
     print_newline();
     ```
   - Fallback: Wenn Registry fehlschlägt → `"Windows (unknown build)"`

3. `CMakeLists.txt` anpassen: `src/os_info.c` zu Sources hinzufügen

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Erwartet: "OS       Windows 11 Pro 26100"
```

---

### Step 4 — GPU NVIDIA `[Must]`

**Ziel:** Alle NVIDIA-Metriken via NVML, dynamisch geladen, graceful skip.

**Dateien:** `src/gpu_nvidia.c`, `src/gpu_nvidia.h`, `include/nvml.h`

**Konkrete Aufgaben:**

1. `include/nvml.h` aus NVIDIA GPU Deployment Kit oder GitHub Repo kopieren (nur Header, kein SDK nötig)

2. `src/gpu_nvidia.h`:
   ```c
   void print_gpu_info(void);
   ```

3. `src/gpu_nvidia.c`:
   - `LoadLibraryA("nvml.dll")` — wenn NULL → `"GPU      [NVIDIA driver not found, skipping]\n"` + return
   - `GetProcAddress` für: `nvmlInit_v2`, `nvmlShutdown`, `nvmlDeviceGetCount`, `nvmlDeviceGetHandleByIndex`, `nvmlDeviceGetName`, `nvmlDeviceGetTemperature`, `nvmlDeviceGetUtilizationRates`, `nvmlDeviceGetMemoryInfo`, `nvmlDeviceGetClockInfo`, `nvmlDeviceGetPowerUsage`
   - `nvmlInit_v2()` → `nvmlDeviceGetHandleByIndex(0, &device)`
   - Auslesen:
     - Name: `nvmlDeviceGetName(device, name, sizeof(name))`
     - Core-Takt: `nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &clock)` → MHz
     - Mem-Takt: `nvmlDeviceGetClockInfo(device, NVML_CLOCK_MEM, &clock)` → MHz
     - Auslastung: `nvmlDeviceGetUtilizationRates(device, &util)` → `util.gpu` = %
     - VRAM: `nvmlDeviceGetMemoryInfo(device, &mem)` → `used`/`total` in GB
     - Temperatur: `nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp)` → °C
     - Power: `nvmlDeviceGetPowerUsage(device, &power)` → mW ÷ 1000 = W
   - `nvmlShutdown()` + `FreeLibrary(nvmlDll)`
   - **Output:**
     ```
     print_section("GPU");
     print_value(name); print_newline();
     // Zeile 2: Core X MHz | Mem X MHz | Load X%
     // Zeile 3: VRAM X / X GB | Temp X°C | Power X W
     ```

4. `CMakeLists.txt`: `include/nvml.h` via `target_include_directories`, `src/gpu_nvidia.c` zu Sources

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Mit NVIDIA: GPU-Block mit allen Metriken
# Ohne NVIDIA: "GPU      [NVIDIA driver not found, skipping]"
```

---

### Step 5 — CPU-Modul `[Must]`

**Ziel:** Name, Takt, Load. Rein C (Registry + PDH).

**Dateien:** `src/cpu.c`, `src/cpu.h`

**Konkrete Aufgaben:**

1. `src/cpu.h`:
   ```c
   void print_cpu_info(void);
   ```

2. `src/cpu.c`:
   - **Name + Takt** (Registry):
     - `RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", ...)`
     - `RegQueryValueExA(key, "ProcessorNameString", ...)` → z.B. `"AMD Ryzen 9 7950X 16-Core Processor"`
     - `RegQueryValueExA(key, "~MHz", ...)` → Base-Takt in MHz
   - **Boost-Takt** (optional):
     - `CPUID` EAX=0x15 (max-frequency) oder EAX=0x16 (processor-frequency) wenn verfügbar
     - Fallback: Registry `~MHz` Wert oder `"N/A"`
   - **Load** (PDH):
     - `PdhOpenQueryA(NULL, NULL, &query)`
     - `PdhAddCounterA(query, "\\Processor(_Total)\\% Processor Time", 0, &counter)`
     - `PdhCollectQueryData(query)` → `Sleep(100)` → `PdhCollectQueryData(query)`
     - `PdhGetFormattedCounterValue(counter, &type, NULL, &value)` → `value.doubleValue` = %
     - `PdhCloseQuery(query)`
   - **Output:**
     ```
     print_section("CPU");
     print_value("AMD Ryzen 9 7950X @ 5.7 GHz"); print_newline();
     // Zeile 2: "Load  34%"
     ```

3. `CMakeLists.txt`: `pdh.lib` linken (bereits in Step 1), `src/cpu.c` zu Sources

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Erwartet: CPU-Name + Load in %
```

---

### Step 6 — CPU Temperature & Power `[Must]`

**Ziel:** Temperatur und Power. Primär Perflib Counter (C), Fallback WMI COM (C++).

**Dateien:** `src/cpu_temp.c`, `src/cpu_temp.h`

**Konkrete Aufgaben:**

1. `src/cpu_temp.h`:
   ```c
   // C-Interface (für main.c Aufruf)
   void print_cpu_temp_power(void);
   ```

2. `src/cpu_temp.c` (C-Datei mit Perflib Counter):
   - **Perflib API** (deklariert in `src/perflib.h`, wenn nicht im SDK):
     - `PerfOpenQueryHandle()` → Query-Handle
     - `PerfAddCounters(handle, counters, numCounters)` — Counter für "Thermal Zone Information"
     - `PerfQueryCounterData(handle, ...)` → Roh-Daten
     - `PerfCloseQueryHandle(handle)`
   - **Alternative (einfacher):** WMI COM Fallback in separater `.cpp`-Datei
   - **Output:**
     ```
     print_value("  Load  34%  |  Temp  71°C  |  Power  88 W");
     print_newline();
     ```

3. `src/cpu_temp_wmi.cpp` (C++ Fallback, nur wenn Perflib nicht funktioniert):
   - `CoInitializeEx(NULL, COINIT_MULTITHREADED)`
   - `CoCreateInstance(CLSID_WbemLocator, ...)` → `IWbemLocator`
   - `ConnectServer("ROOT\\WMI")` → `IWbemServices`
   - Query: `SELECT * FROM MSAcpi_ThermalZoneTemperature` → Temperatur in Kelvin / 10 - 273.15
   - Query: `SELECT * FROM Win32_PerfFormattedData_Counters_ThermalZoneInformation` oder `MSAcpi_ThermalZoneTemperature` für Power (wenn verfügbar)
   - `CoUninitialize()`
   - Fehler → jeweiliges Feld = `"N/A"`

4. `CMakeLists.txt`:
   - `src/cpu_temp.c` zu C-Sources
   - `src/cpu_temp_wmi.cpp` zu C++-Sources (nur MSVC)
   - `wbemuuid.lib`, `ole32.lib`, `oleaut32.lib` linken (bereits in Step 1)

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Erwartet: "Load  34%  |  Temp  71°C  |  Power  88 W"
# Oder bei N/A: "Load  34%  |  Temp  N/A  |  Power  N/A"
```

---

### Step 7 — RAM-Modul `[Must]`

**Ziel:** Used/Total via C, Takt/Latenz optional via WMI COM.

**Dateien:** `src/memory.c`, `src/memory.h`, `src/memory_spd.cpp`, `src/memory_spd.h`

**Konkrete Aufgaben:**

1. `src/memory.h`:
   ```c
   void print_memory_info(void);
   ```

2. `src/memory.c` (rein C):
   - `GlobalMemoryStatusEx(&memStatus)`
   - `ullTotalPhys` = Total, `ullTotalPhys - ullAvailPhys` = Used
   - Umrechnung in GB: `/(1024.0 * 1024.0 * 1024.0)`
   - **Output:**
     ```
     print_section("RAM");
     print_value("22.1 / 32.0 GB"); print_newline();
     ```

3. `src/memory_spd.cpp` (C++, optional):
   - WMI COM: `SELECT * FROM Win32_PhysicalMemory`
   - `Speed` → MHz, `ConfiguredClockSpeed` → MHz, `CASLatency` → CL
   - Fehler → `"N/A"` für jedes Feld
   - **Output (angehängt):**
     ```
     "  |  6000 MHz  CL30"
     ```

4. `CMakeLists.txt`: `src/memory.c` + `src/memory_spd.cpp` zu Sources

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Erwartet: "RAM      22.1 / 32.0 GB  |  6000 MHz  CL30"
# Ohne Admin: "RAM      22.1 / 32.0 GB"
```

---

### Step 8 — Storage-Modul `[Must]`

**Ziel:** Disk Read/Write (500 ms), NVMe Temp via DeviceIoControl.

**Dateien:** `src/storage.c`, `src/storage.h`

**Konkrete Aufgaben:**

1. `src/storage.h`:
   ```c
   void print_storage_info(void);
   ```

2. `src/storage.c` (rein C):
   - **I/O Speed** (PDH):
     - `PdhAddCounterA(query, "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", ...)`
     - `PdhAddCounterA(query, "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", ...)`
     - `PdhCollectQueryData` → `Sleep(500)` → `PdhCollectQueryData`
     - Delta auslesen, in MB/s umrechnen: `delta / 1024 / 1024 / 0.5`
     - **Wenn beide Werte == 0 → Read/Write-Zeile NICHT ausgeben**
   - **NVMe Temp** (DeviceIoControl):
     - `CreateFileA("\\\\.\\PhysicalDrive0", FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, ...)`
     - `DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, ...)` → `STORAGE_DEVICE_DESCRIPTOR` → Name
     - SMART-Daten: `DeviceIoControl(hDevice, IOCTL_ATA_PASS_THROUGH, ...)` → `ATA_SMART_ATTRIBUTE` → Temperatur
     - Fallback: Wenn kein Zugriff → `"NVMe Temp  N/A"`
   - **Output:**
     ```
     print_section("Disk");
     // Wenn I/O > 0:
     print_value("Read  2450 MB/s  |  Write  1820 MB/s"); print_newline();
     // Immer:
     print_value("NVMe Temp  38°C"); print_newline();
     ```

3. `CMakeLists.txt`: `src/storage.c` zu Sources

**Verification:**
```powershell
.\build\Release\hardfetch.exe
# Unter Last: "Disk     Read  2450 MB/s  |  Write  1820 MB/s"
#              "NVMe Temp  38°C"
# Im Leerlauf: "Disk     NVMe Temp  38°C" (nur Temp-Zeile)
```

---

### Step 9 — Network-Modul `[Must]`

**Ziel:** Upload/Download nur bei `--net`, sonst komplett hidden.

**Dateien:** `src/network.c`, `src/network.h`

**Konkrete Aufgaben:**

1. `src/network.h`:
   ```c
   void print_network_info(void);  // prüft intern argc/argv oder Flag
   ```

2. `src/network.c` (rein C):
   - Wenn kein `--net` Flag → return (keine Ausgabe)
   - `GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, &buffer, &bufferLen)`
   - Retry-Loop (bis zu 4 Versuche) bei `ERROR_BUFFER_OVERFLOW`
   - Interfaces iterieren, `IfOperStatus == IfOperStatusUp` filtern
   - `GetIfEntry2(&row)` → `row.statistics.InOctets` / `OutOctets` auslesen
   - 500 ms Sleep, erneut auslesen, Delta bilden
   - MB/s = `delta / 1024 / 1024 / 0.5`
   - Summe aller aktiven Interfaces
   - **Output:**
     ```
     print_section("Net");
     print_value(" 12.4 MB/s  |   84.1 MB/s"); print_newline();
     ```

3. `CMakeLists.txt`: `src/network.c` zu Sources, `iphlpapi.lib` linken (bereits in Step 1)

**Verification:**
```powershell
.\build\Release\hardfetch.exe --net
# Erwartet: "Net       12.4 MB/s  |   84.1 MB/s"

.\build\Release\hardfetch.exe
# Erwartet: Keine Net-Zeile
```

---

### Step 10 — Integration & Main Loop `[Must]`

**Ziel:** Alle Module zusammenführen, Reihenfolge, Farben, Trenner.

**Dateien:** `src/main.c` (anpassen)

**Konkrete Aufgaben:**

1. `main.c` anpassen:
   ```c
   int main(int argc, char* argv[]) {
       // Flags parsen
       int show_net = 0;
       int show_version = 0;
       int show_help = 0;
       for (int i = 1; i < argc; i++) {
           if (strcmp(argv[i], "--net") == 0) show_net = 1;
           if (strcmp(argv[i], "--version") == 0) show_version = 1;
           if (strcmp(argv[i], "--help") == 0) show_help = 1;
       }

       if (show_version) { printf("hardfetch v0.1\n"); return 0; }
       if (show_help) { /* Usage */ return 0; }

       output_init();

       // Header
       print_section("hardfetch v0.1");
       print_newline();

       // Module in Reihenfolge
       print_os_info();
       print_newline();
       print_cpu_info();
       print_cpu_temp_power();
       print_newline();
       print_gpu_info();
       print_newline();
       print_memory_info();
       print_newline();
       print_storage_info();
       print_newline();
       if (show_net) {
           print_network_info();
           print_newline();
       }

       return 0;
   }
   ```

2. Kein `Sleep`, kein Loop — einmalig ausführen und beenden
3. Exit Code 0 (Erfolg), Exit Code 1 nur bei kritischem Fehler

**Verification:**
```powershell
.\build\Release\hardfetch.exe --net
# Komplette Ausgabe wie im Ziel-Output
```

---

### Step 11 — Statischer Build & Optimierung `[Must]`

**Ziel:** Ein `.exe` das auf jedem Windows 10/11-PC läuft, ohne Installation.

**Dateien:** `CMakeLists.txt` (anpassen)

**Konkrete Aufgaben:**

1. Release-Profil prüfen: `/O2 /MT /DNDEBUG /W4`
2. Alle Windows-System-Libs korrekt linken:
   - `iphlpapi.lib` (Network)
   - `pdh.lib` (Performance Counter)
   - `wbemuuid.lib` (WMI/COM)
   - `ole32.lib`, `oleaut32.lib`, `uuid.lib` (COM)
   - `ntdll.lib` (NT Native Registry)
   - `advapi32.lib` (RegOpenKeyEx Fallback)
3. `nvml.dll` NICHT statisch linken — bleibt dynamischer Load
4. `target_link_options` für Console-Subsystem prüfen
5. Suppress libstdc++: `set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")`

**Verification:**
```powershell
cmake -B build -A x64
cmake --build build --config Release
# .exe Größe ca. 200-800 KB
# Auf frischem Windows 11 VM testen:
.\build\Release\hardfetch.exe --version
.\build\Release\hardfetch.exe
.\build\Release\hardfetch.exe --net
```

---

### Step 12 — GitHub Release & CI `[Nice]`

**Ziel:** Automatisches Build & Release auf GitHub.

**Dateien:** `.github/workflows/release.yml`

**Konkrete Aufgaben:**

1. GitHub Actions Workflow:
   - Trigger: `push` auf Tag `v*`
   - Runner: `windows-latest`
   - Steps: Checkout → CMake Configure → CMake Build → Upload Artifact
2. Build-Command: `cmake -B build -A x64 && cmake --build build --config Release`
3. Release-Asset: `build/Release/hardfetch.exe`
4. Kein Winget/Scoop/Chocolatey im MVP — nur `.exe` als Release-Asset

**Verification:**
- Tag `v0.1.0` pushen → Workflow grün → Release-Asset downloaden → auf sauberem PC testen

---

## Abhängigkeitsgraph

```
Step 1 (CMake + main.c)
  ↓
Step 2 (Output-System)
  ↓
Step 3 (OS) ─┐
  ↓           │
Step 4 (GPU) ─┤
  ↓           │
Step 5 (CPU) ─┤ parallel-ish, aber Step 3 zuerst
  ↓           │
Step 6 (Temp)─┤
  ↓           │
Step 7 (RAM) ─┤
  ↓           │
Step 8 (Disk)─┘
  ↓
Step 9 (Network)
  ↓
Step 10 (Integration)
  ↓
Step 11 (Statischer Build)
  ↓
Step 12 (GitHub Release)
```

---

## Technische Details

### Perflib Counter (CPU Temp)

```c
// Deklaration (wenn nicht im SDK)
typedef PVOID (WINAPI *PFn_PerfOpenQueryHandle)(PVOID);
typedef ULONG (WINAPI *PFn_PerfAddCounters)(PVOID, PPERF_COUNTER_IDENTIFIER, ULONG);
typedef ULONG (WINAPI *PFn_PerfQueryCounterData)(PVOID, PPERF_DATA_HEADER*, PPERF_COUNTER_DATA*, ULONG);
typedef ULONG (WINAPI *PFn_PerfCloseQueryHandle)(PVOID);

// Dynamisch laden aus advapi32.dll oder perflib
HMODULE perflib = LoadLibraryA("perflib.dll");
```

### NVMe Temperatur (DeviceIoControl)

```c
// IOCTL_ATA_PASS_THROUGH (0x2280C1)
// ATA Command: SMART READ DATA (0xB0)
// Feature: SMART READ DATA (0xD0)
// LBA: 0xC2F1 (SMART attribute page)
// Result → ATTRIBUTE_TABLE[194] → RAW_VALUE → Temperatur in °C
```

### WMI COM Fallback (C++)

```cpp
// Minimal für Temp/Power
CoInitializeEx(NULL, COINIT_MULTITHREADED);
CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&locator);
locator->ConnectServer(L"ROOT\\WMI", NULL, NULL, NULL, 0, NULL, NULL, &services);
services->ExecQuery(L"WQL", L"SELECT * FROM MSAcpi_ThermalZoneTemperature", ...);
// Temperatur = (value - 27315) / 100.0
```

---

## Post-Release Iterationen

| Feature | Wann |
|---------|------|
| `--lhm` Flag (LibreHardwareMonitor Fallback) | v0.2 |
| AMD GPU via ADL (AMD Display Library) | v0.3 |
| Config-File (`hardfetch.jsonc`) | v0.3 |
| Pro-Kern-Auslastung | v0.4 |
| Linux-Port | nie (Out-of-Scope) |
