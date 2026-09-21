#!/usr/bin/env bash
# Build (and optionally install) moofetch on this machine.
#
#   ./scripts/install-local.sh [--prefix /usr/local] [--no-install] [--with-converters]
#
#   --prefix DIR       install prefix (default: /usr/local)
#   --no-install       only build, do not install
#   --with-converters  also install ImageMagick and chafa (for tools/gif2anim)
#                      when a supported package manager is detected
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
prefix="/usr/local"
do_install=1
with_converters=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) prefix="$2"; shift 2 ;;
        --no-install) do_install=0; shift ;;
        --with-converters) with_converters=1; shift ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

missing=()
for tool in cmake cc python3; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
if [[ ${#missing[@]} -gt 0 ]]; then
    echo "Missing build tools: ${missing[*]}" >&2
    echo "Install them first, e.g.:" >&2
    echo "  Arch/CachyOS:   sudo pacman -S --needed base-devel cmake python" >&2
    echo "  Debian/Ubuntu:  sudo apt install build-essential cmake python3" >&2
    echo "  Fedora:         sudo dnf install gcc cmake python3" >&2
    exit 1
fi

if [[ $with_converters -eq 1 ]]; then
    if command -v pacman >/dev/null; then
        sudo pacman -S --needed --noconfirm imagemagick chafa
    elif command -v apt >/dev/null; then
        sudo apt install -y imagemagick chafa
    elif command -v dnf >/dev/null; then
        sudo dnf install -y ImageMagick chafa
    else
        echo "No supported package manager found; install ImageMagick and chafa manually." >&2
    fi
fi

cd "$root"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo
if [[ $do_install -eq 1 ]]; then
    sudo cmake --install build --prefix "$prefix"
    echo "Installed to $prefix/bin/moofetch. Try:"
    echo "  moofetch --logo-type animation --logo cachyos_rotate"
else
    echo "Built $root/build/moofetch. Try:"
    echo "  $root/build/moofetch --logo-type animation --logo cachyos_rotate"
fi
