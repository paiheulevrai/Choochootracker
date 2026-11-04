#!/bin/bash
set -euo pipefail

# Run from tracker. The official framework contains both desktop architectures.
framework="platforms/macos/Frameworks/SDL2.framework"
version=2.32.10
digest=4a7ac31640d70214e848f994be8a12849c0f97918a7e6c2e27a40036166d1a7f

if [[ "${1:-}" == --download ]]; then
    temp=$(mktemp -d)
    mounted=0
    cleanup() {
        if [[ "$mounted" == 1 ]]; then hdiutil detach "$temp/mount" -quiet; fi
        rm -f "$temp/SDL2.dmg"
        if [[ -d "$temp/mount" ]]; then rmdir "$temp/mount"; fi
        rmdir "$temp"
    }
    trap cleanup EXIT
    curl --fail --location --retry 3 \
        "https://github.com/libsdl-org/SDL/releases/download/release-$version/SDL2-$version.dmg" \
        -o "$temp/SDL2.dmg"
    printf '%s  %s\n' "$digest" "$temp/SDL2.dmg" | shasum -a 256 -c -
    mkdir "$temp/mount"
    hdiutil attach "$temp/SDL2.dmg" -readonly -nobrowse -mountpoint "$temp/mount" -quiet
    mounted=1
    if [[ -e "$framework" ]]; then
        echo "Refusing to overwrite $framework; remove it explicitly to reinstall." >&2
        exit 1
    fi
    cp -R "$temp/mount/SDL2.framework" "$framework"
elif [[ $# != 0 ]]; then
    echo "Usage: bash platforms/macos/check-sdl2.sh [--download]" >&2
    exit 1
fi

test -f "$framework/SDL2"
test -d "$framework/Headers"
lipo "$framework/SDL2" -verify_arch arm64 x86_64
otool -L "$framework/SDL2"
echo "SDL2 universal framework ready."
