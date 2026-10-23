#!/usr/bin/env bash
set -euo pipefail

os=$(uname -s)
arch=$(uname -m)

case "$os-$arch" in
    Linux-x86_64)  n="hardfetch" ;;
    Darwin-arm64)  n="hardfetch-macos-arm64" ;;
    Darwin-x86_64) n="hardfetch-macos-x64" ;;
    *) echo "Fehler: Keine Binärdatei für $os $arch verfügbar." >&2; exit 1 ;;
esac

url="https://github.com/00vez/hardfetch/releases/latest/download"
dest="$HOME/.local/bin"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Download (erzwingt TLS 1.2+)
curl --proto '=https' --tlsv1.2 -fsSL "$url/$n" -o "$tmp/$n"
curl --proto '=https' --tlsv1.2 -fsSL "$url/SHA256SUMS" -o "$tmp/SHA256SUMS"

cd "$tmp"

# Hash-Prüfung (unterstützt 'hash *file' und 'hash file')
if command -v sha256sum >/dev/null 2>&1; then
    check_cmd="sha256sum"
else
    check_cmd="shasum -a 256"
fi

if ! grep -E "[ *]$n$" SHA256SUMS | $check_cmd -c - >/dev/null 2>&1; then
    echo "Fehler: Integritätsprüfung fehlgeschlagen! Download manipuliert oder unvollständig." >&2
    exit 1
fi

mkdir -p "$dest"
install -m 755 "$tmp/$n" "$dest/hardfetch"

echo "Erfolgreich nach $dest/hardfetch installiert."

# PATH-Prüfung
if [[ ":$PATH:" != *":$dest:"* ]]; then
    echo -e "\n\033[0;33mHinweis:\033[0m $dest ist nicht in deinem PATH."
    echo "Füge folgende Zeile zu deiner ~/.bashrc oder ~/.zshrc hinzu:"
    echo "    export PATH=\"\$HOME/.local/bin:\$PATH\""
fi
