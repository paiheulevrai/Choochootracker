#!/bin/sh
# Copy the checked-in guide into the existing static web output.
set -eu
guide_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
target=${1:-"$guide_dir/../../web/dist/user-guide"}
mkdir -p "$target"
cp "$guide_dir/index.html" "$target/index.html"
cp -R "$guide_dir/assets" "$target/"
printf 'Published visual guide to %s\n' "$target"
