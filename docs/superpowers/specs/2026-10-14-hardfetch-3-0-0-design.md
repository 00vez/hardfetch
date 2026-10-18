=== hardfetch 3.0.0 — Design-Spezifikation ===
Status: Entwurf, Nutzer bestätigt (2026-10-14)
Ziel: v0.3.0, Windows 11 x86_64, MSVC /MT, C11/C++17, keine neuen Abhängigkeiten

=== 1. Layout und Format (output.c/h) ===
- Label: %-11s (aktuell %-10s + 2 Leerzeichen). Wert ab Spalte 12.
- Detail: Einrückung 11 Leerzeichen (aktuell 12).
- Trenner-Literal: U+00B7 (\xC2\xB7), Konstante OUTPUT_SEP.
- Header: print_header("hardfetch v0.3.0 · v@DESKTOP-EE1BP3T") — eine Zeile.
- Farbe: detail-Zeilen bleiben grau (90m) — Zieloutput zeigt Werte als weiß/grün; das bleibt konsistent, weil die Zielbilder nur die Hauptzeile zeigen und Detailwerte als Unterpunkte gelten.

=== 2. Module (Reihenfolge) ===
Header → OS → Build → Board → Shell → Uptime / CPU → GPU (mit iGPU als GPU 2) → RAM → Disk (C:) → Display → IP (bestes Interface) / -a: alle + Public

=== 3. Pro Modul ===
OS (os_info.c): Windows 11 Pro 25H2 (DisplayVersion) + Arch x86_64. Trenner ·. Keine Klammern.
Build (kernel.c, umgebaut): Reg "CurrentBuildNumber" + "." + "UBR" → "26200.9457". Datei bleibt, print_kernel_info wird print_build_info.
Board (host.c): Label "Host" → "Board". BaseBoardProduct bleibt. Optional BaseBoardManufacturer.
Shell (shell.c): Unverändert. PowerShell 7.6.6.
Uptime (uptime.c): Format "5h 03m", "3d 04h 12m", "12m". Keine Änderung der Quelle.
CPU (cpu.c): Mainline nur ProcessorNameString (ohne -Core-Strip? Zieloutput zeigt vollen Namen). Detail: "4.19 GHz · 12% Load". Load: Sleep(20) → Sleep(150) für Stabilität. Frequenz: PDH \Processor Performance(PPM_Processor_0)\Processor Frequency nur, keine % of Maximum Frequency-Hochrechnung (cpu.c:42-51 entfernt). print_cpu_temp_power-Aufruf entfernt.
RAM (memory.c + memory_spd.cpp): "10.0 / 31.1 GiB · 32%" + Continuation "6000 MHz". CL bleibt 0 (get_memory_speed liefert ConfiguredClockSpeed, CAS nie gefüllt — nicht geändert, nur nicht angezeigt).
GPU (gpu.cpp): NVML: nvmlDeviceGetClockInfo statt GetMaxClockInfo (gpu.cpp:29, 53, 62). Mainline nur Name. Detail: 3 Zeilen (Load · Temp · Power / VRAM / Core · Memory). iGPU: GPU 2 als eigene Sektion, ohne [Integrated]/[Discrete]-Marker.
Disk (storage.c): Label "Disk (C:)", C: bleibt hartkodiert. Format "3.16 / 3.64 TiB · 87%" ohne Klammern.
IP (ip_addr.c): Default: bestes Interface (WLAN > Ethernet, AF_INET, OperStatusUp, FriendlyName max 10 Zeichen). -a: alle aktiven Interfaces + public_ip.c Aufruf.
Display (display.c, neu): EnumDisplaySettingsW für PelsWidth/Height und DisplayFrequency. Monitor-Name und Diagonale aus WMI root\wmi WmiMonitorBasicDisplayParams (MaxHorizontalImageSize/MaxVerticalImageSize → cm → Zoll). Wenn leer: Diagonale weglassen.
Public IP (public_ip.c): Nur bei -a aufgerufen. --net entfällt.

=== 4. Performance (gemessen) ===
Aktuell (kalt): 398 ms. WMI-Teile (warm): VideoController 21ms, PhysicalMemory 8ms, ThermalZone 46ms. CPU-Temp-Timeout (150ms) entfällt durch Entfernung aus Windows-Pfad. WMI-Connector-Sharing: memory_spd.cpp und gpu.cpp teilen IWbemServices (ein ConnectServer statt zwei). Ziel: <180 ms (kein Thread-Umbau, kein TLS).

=== 5. Grenzen ===
- CPU-Temp: entfällt im Windows-Output (Desktop-AMD keine ACPI-Zone), Datei bleibt für POSIX.
- Diagonale: WMI root\wmi WmiMonitorBasicDisplayParams. Wenn leer → weglassen, kein EDID-Parser.
- Disk-I/O-Rate: nicht in 3.0.0.
- Parallelisierung: nicht in 3.0.0 (kein Gewinn gegenüber WMI-Share + Temp-Entfernung).
- Kernel-Zeile: entfällt aus Output, Datei bleibt (umgebaut zu print_build_info).
- Tote Helfer (print_item etc.) und get_os_* bleiben als Dateien, nicht gelöscht.
