& {
    $ErrorActionPreference = "Stop"
    $ProgressPreference = 'SilentlyContinue'

    # Architektur prüfen (Release ist x64, Windows ARM emuliert x64, aber wir warnen)
    if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
        Write-Host "Hinweis: Du nutzt Windows ARM64. Die x64-Binärdatei wird per Emulation ausgeführt." -ForegroundColor Yellow
    }

    $url = "https://github.com/00vez/hardfetch/releases/latest/download"
    $dest = "$env:USERPROFILE\.local\bin"
    $tmp = New-Item -ItemType Directory -Force -Path (Join-Path $env:TEMP ([System.IO.Path]::GetRandomFileName()))

    try {
        Write-Host "Lade hardfetch.exe herunter..."
        Invoke-WebRequest -Uri "$url/hardfetch.exe" -OutFile "$tmp\hardfetch.exe"
        Invoke-WebRequest -Uri "$url/SHA256SUMS" -OutFile "$tmp\SHA256SUMS"

        Write-Host "Verifiziere Prüfsumme..."
        $sumLine = Select-String -Path "$tmp\SHA256SUMS" -Pattern '\*hardfetch\.exe$' | Select-Object -First 1
        if (-not $sumLine) { throw "hardfetch.exe nicht in SHA256SUMS gefunden." }
        
        $expectedHash = $sumLine.Line.Split(" ")[0].Trim().ToLower()
        $actualHash = (Get-FileHash "$tmp\hardfetch.exe" -Algorithm SHA256).Hash.ToLower()

        if ($expectedHash -ne $actualHash) {
            throw "Integritätsprüfung fehlgeschlagen! Die Datei ist möglicherweise manipuliert."
        }
        Write-Host "Prüfsumme OK."

        Write-Host "Installiere nach $dest..."
        if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest | Out-Null }
        
        # Versuche die Datei zu verschieben, fange laufende Prozesse ab
        try {
            Move-Item -Path "$tmp\hardfetch.exe" -Destination "$dest\hardfetch.exe" -Force
        } catch {
            throw "Fehler beim Überschreiben. Läuft hardfetch.exe noch im Hintergrund? Bitte schließe es und versuche es erneut."
        }

        Write-Host "Erfolgreich nach $dest\hardfetch.exe installiert."
        
        if ($env:PATH -notmatch [regex]::Escape($dest)) {
            Write-Host "`nHinweis: $dest ist nicht in deinem PATH." -ForegroundColor Yellow
            Write-Host "Füge es zu den Umgebungsvariablen hinzu, um 'hardfetch' global auszuführen."
        }
    } finally {
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    }
}
