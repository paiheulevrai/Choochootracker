#!/bin/bash
set -eu

deb=$1
repository=$2
: "${REPO_KEY:?Set REPO_KEY to the GPG signing key identifier}"

if ! mkdir "$repository"; then
  echo "Cannot create repository '$repository' (it must not already exist)" >&2
  exit 1
fi

mkdir -p "$repository/pool/main/c/choochootracker" \
  "$repository/dists/stable/main/binary-arm64"
cp "$deb" "$repository/pool/main/c/choochootracker/"
(
  cd "$repository"
  apt-ftparchive packages pool > dists/stable/main/binary-arm64/Packages
  gzip -n -k dists/stable/main/binary-arm64/Packages
  cd dists/stable
  apt-ftparchive \
    -o APT::FTPArchive::Release::Origin=ChooChooTracker \
    -o APT::FTPArchive::Release::Label=ChooChooTracker \
    -o APT::FTPArchive::Release::Suite=stable \
    -o APT::FTPArchive::Release::Codename=stable \
    -o APT::FTPArchive::Release::Architectures=arm64 \
    -o APT::FTPArchive::Release::Components=main \
    release . > Release
  gpg --batch --yes --local-user "$REPO_KEY" --clearsign --output InRelease Release
  gpg --batch --yes --local-user "$REPO_KEY" --armor --detach-sign --output Release.gpg Release
)
echo "Signed local development repository: $repository"
