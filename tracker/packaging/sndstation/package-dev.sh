#!/bin/bash
set -eu

binary=$1
version=${2/-alpha/~alpha}
output="build/sndstation/choochootracker-sndstation_${version}_arm64.deb"
mkdir -p build/sndstation
stage=$(mktemp -d "${TMPDIR:-/tmp}/cct-sndstation-package.XXXXXXXX")
trap 'rm -rf -- "$stage"' EXIT

mkdir -p "$stage/DEBIAN" "$stage/usr/bin" \
  "$stage/usr/share/choochootracker/common" \
  "$stage/usr/share/sndstation/apps" \
  "$stage/usr/share/doc/choochootracker-sndstation"
install -m 755 "$binary" "$stage/usr/bin/choochootracker-sndstation"
printf '%s\n' '#!/bin/sh' 'export SDL_RENDER_DRIVER=software' \
  'exec /usr/bin/choochootracker-sndstation "$@"' \
  > "$stage/usr/bin/choochootracker-sndstation-software"
chmod 755 "$stage/usr/bin/choochootracker-sndstation-software"
cp -R packaging/common/. "$stage/usr/share/choochootracker/common/"
install -m 644 ../docs/USER_MANUAL.md ../LICENSE \
  "$stage/usr/share/doc/choochootracker-sndstation/"
install -m 644 ../chipnomad_lib/external/insert_fx/LICENSE-JM-Formant \
  "$stage/usr/share/doc/choochootracker-sndstation/"
cp packaging/portmaster/license/* "$stage/usr/share/doc/choochootracker-sndstation/"
find "$stage/usr/share" -type d -exec chmod 755 {} +
find "$stage/usr/share" -type f -exec chmod 644 {} +

cat > "$stage/DEBIAN/control" <<EOF
Package: choochootracker-sndstation
Version: $version
Architecture: arm64
Maintainer: ChooChooTracker contributors <noreply@github.com>
Depends: libc6 (>= 2.41), libgcc-s1, libstdc++6, libasound2t64, libsdl2-2.0-0
Section: sound
Priority: optional
Sndstation-Kind: app
Description: ChooChooTracker SNDStation development build
 Experimental build; Smart Pro local volume controls and display rotation are
 included, but rendering performance is still under investigation.
EOF
cat > "$stage/usr/share/sndstation/apps/choochootracker.json" <<'EOF'
{
  "name": "ChooChooTracker (development)",
  "description": "Experimental tracker build",
  "exec": ["/usr/bin/choochootracker-sndstation"]
}
EOF

cat > "$stage/usr/share/sndstation/apps/choochootracker-software.json" <<'EOF'
{
  "name": "ChooChooTracker (software test)",
  "description": "Experimental software renderer",
  "exec": ["/usr/bin/choochootracker-sndstation-software"]
}
EOF

dpkg-deb --build --root-owner-group "$stage" "$output"
echo "Development package: $output"
