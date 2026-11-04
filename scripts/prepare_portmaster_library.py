"""Prepare PortMaster Library sources from a complete, checksummed release ZIP."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import zipfile
import xml.etree.ElementTree as ET


PORT = "choochootracker"
LAUNCHER = "ChooChooTracker.sh"
METADATA = ("port.json", "README.md", "screenshot.png", "cover.png", "gameinfo.xml")


def checksum_entry(manifest, filename):
    matches = []
    for line in manifest.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-fA-F]{64}) [ *](.+)", line)
        if not match:
            raise ValueError(f"Invalid checksum line: {line!r}")
        if match[2] == filename:
            matches.append(match[1].lower())
    if len(matches) != 1:
        raise ValueError(f"Expected one checksum for {filename}, got {len(matches)}")
    return matches[0]


def update_checksum(archive, manifest):
    checksum_entry(manifest, archive.name)
    lines = manifest.read_bytes().splitlines(keepends=True)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest().encode("ascii")
    for index, line in enumerate(lines):
        if line[66:].rstrip(b"\r\n") == archive.name.encode("utf-8"):
            lines[index] = digest + line[64:]
    manifest.write_bytes(b"".join(lines))


def pr_body(tag, source_sha, run_url, digest):
    return f"""## Game Information
- **New Port for**: ChooChooTracker
- **URL**: https://github.com/paiheulevrai/Choochootracker
- Testing Thread URL: private PortMaster Discord thread; maintainer can provide access.

Ready-to-run native ARM64 music tracker. No additional game data or runtime.

## Authorship & Testing
- [ ] I reviewed the AI-assisted packaging/workflow changes and understand the script.
- [ ] I can explain every non-standard launcher line.

The existing launcher uses PortMaster helpers. The ArkOS audio fallback preserves
hardware volume controls; ROCKNIX uses gptokeyb2 to release Select/Shift correctly.
Other firmware retains gptokeyb. The executable bit is restored after extraction.

### CFW Tests
Historical testing reported by the maintainer (2026-11-04), not certification of
this rebuilt package: ROCKNIX on RG353V (libmali and panfrost), dArkOS, ArkOS,
muOS, Knulli and AmberELEC. muOS and some Knulli/ROCKNIX checks are community
reports; AmberELEC details remain in the private testing discussion.
Confirm the final package before checking these boxes:
- [ ] AmberELEC
- [ ] dArkOS
- [ ] muOS
- [ ] Knulli
- ROCKNIX: [ ] Libmali / [ ] Panfrost / [ ] Adreno (optional)
- [ ] Other: ArkOS

### Resolution Tests
Historical reports: 640x480 and 1280x720 personally tested; 1024x768 and 720x720
reported by users. 320x240 was not tested; RG Nano 240x240 is a different build.
- [ ] 640x480
- [ ] 720x720 (optional)
- [ ] Higher resolutions (1280x720)
- [ ] 480x320 (optional)

## Package provenance
- Release: {tag}
- Source commit: {source_sha}
- Build: {run_url}
- ZIP SHA-256: `{digest}`

Only `ports/choochootracker/` is included. Please review the testing checklist
and AI-assistance declaration before submitting.
"""


def prepare(archive, manifest, output, tag, source_sha, run_url):
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != checksum_entry(manifest, archive.name):
        raise ValueError(f"Checksum mismatch: {archive}")
    if output.exists() and any(output.iterdir()):
        raise ValueError(f"Output must be empty: {output}")

    with zipfile.ZipFile(archive) as package:
        files = {}
        for info in package.infolist():
            path = PurePosixPath(info.filename)
            mode = info.external_attr >> 16
            if (path.is_absolute() or ".." in path.parts or "\\" in info.orig_filename
                    or ":" in info.filename or stat.S_ISLNK(mode)):
                raise ValueError(f"Unsafe archive path: {info.filename}")
            if info.is_dir():
                continue
            if (str(path) != info.filename or info.orig_filename != info.filename
                    or info.filename in files):
                raise ValueError(f"Non-canonical or duplicate path: {info.filename}")
            if path.parts[0] != PORT and info.filename != LAUNCHER:
                raise ValueError(f"Unexpected package entry: {info.filename}")
            if info.file_size > 90 * 1024 * 1024:
                raise ValueError(f"File requires upstream chunking: {info.filename}")
            if re.match(r"lib(SDL|c[.-]|stdc\+\+|pthread|GL|EGL)", path.name):
                raise ValueError(f"Do not bundle system libraries: {info.filename}")
            files[info.filename] = package.read(info)

    required = [LAUNCHER, f"{PORT}/choochootracker.aarch64",
                f"{PORT}/USER_MANUAL.md", f"{PORT}/choochootracker.gptk"]
    required += [f"{PORT}/{name}" for name in METADATA]
    required += [f"{PORT}/instruments/FACTORY/{name}" for name in ("catalog.tsv", "builtins.tsv")]
    for name in required:
        if not files.get(name):
            raise ValueError(f"Missing package file: {name}")
    binary = files[f"{PORT}/choochootracker.aarch64"]
    if binary[:6] != b"\x7fELF\x02\x01" or binary[18:20] != b"\xb7\x00":
        raise ValueError("Expected a little-endian AArch64 ELF64 executable")
    metadata = json.loads(files[f"{PORT}/port.json"])
    if (metadata.get("version") != 4 or metadata.get("name") != f"{PORT}.zip"
            or metadata.get("items") != [LAUNCHER, PORT]
            or metadata.get("attr", {}).get("arch") != ["aarch64"]):
        raise ValueError("port.json does not describe this ARM64 package")
    game = ET.fromstring(files[f"{PORT}/gameinfo.xml"]).find("game")
    if (game is None or game.findtext("path") != f"./{LAUNCHER}"
            or game.findtext("image") != f"./{PORT}/cover.png"
            or not re.fullmatch(r"\d{8}T\d{6}", game.findtext("releasedate") or "")):
        raise ValueError("gameinfo.xml paths/date do not match the package")
    screenshot = files[f"{PORT}/screenshot.png"]
    if (screenshot[:8] != b"\x89PNG\r\n\x1a\n"
            or int.from_bytes(screenshot[16:20], "big") != 640
            or int.from_bytes(screenshot[20:24], "big") != 480):
        raise ValueError("Library screenshot must be a 640x480 PNG")
    if not any(name.startswith(f"{PORT}/licenses/") for name in files):
        raise ValueError("Package has no licenses")

    port_dir = output / "ports" / PORT
    written = set()
    for name, data in files.items():
        parts = PurePosixPath(name).parts
        if len(parts) == 2 and parts[1] in METADATA:
            relative = parts[1]
        elif "licenses" in parts:
            relative = f"{PORT}/licenses/LICENSE." + ".".join(parts[1:]) + ".txt"
        else:
            relative = name
        if relative.casefold() in written:
            raise ValueError(f"Output filename collision: {relative}")
        written.add(relative.casefold())
        if relative.endswith((".sh", ".gptk")):
            data = data.replace(b"\r\n", b"\n")
        target = port_dir / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        target.chmod(0o644)
    (output / "PR_BODY.md").write_bytes(
        pr_body(tag, source_sha, run_url, digest).encode("utf-8"))
    (output / "provenance.json").write_text(json.dumps({
        "tag": tag, "source_sha": source_sha, "run_url": run_url,
        "archive": archive.name, "sha256": digest,
    }, indent=2) + "\n", encoding="utf-8")
    return port_dir


def import_port(prepared, repository):
    root = subprocess.check_output(
        ["git", "-C", str(repository), "rev-parse", "--show-toplevel"], text=True).strip()
    if Path(root).resolve() != repository.resolve():
        raise ValueError("Specify the root of the PortMaster-New clone")
    if not (repository / "SOURCE_SETUP.txt").is_file():
        raise ValueError("Expected a PortMaster-New clone with SOURCE_SETUP.txt checked out")
    source = prepared / "ports" / PORT
    if not (source / "port.json").is_file() or source.is_symlink():
        raise ValueError("Prepared Library port is missing")
    destination = repository / "ports" / PORT
    if source.resolve() == repository.resolve() or repository.resolve() in source.resolve().parents:
        raise ValueError("Prepared input must be outside the destination")
    if destination.parent.resolve() != repository.resolve() / "ports":
        raise ValueError("The destination ports directory must not be a symlink")
    dirty = subprocess.check_output(
        ["git", "-C", str(repository), "status", "--porcelain",
         "--untracked-files=all", "--ignored", "--", f"ports/{PORT}"], text=True)
    if dirty.strip() or destination.is_symlink():
        raise ValueError(f"Refusing to overwrite local changes in {destination}")
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)
    print(f"Imported {destination}; no commit, push or PR was performed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare")
    prep.add_argument("--archive", required=True, type=Path)
    prep.add_argument("--checksums", required=True, type=Path)
    prep.add_argument("--output", required=True, type=Path)
    prep.add_argument("--tag", required=True)
    prep.add_argument("--source-sha", required=True)
    prep.add_argument("--run-url", required=True)
    sync = commands.add_parser("import")
    sync.add_argument("--prepared", required=True, type=Path)
    sync.add_argument("--repository", required=True, type=Path)
    sums = commands.add_parser("update-checksum")
    sums.add_argument("--archive", required=True, type=Path)
    sums.add_argument("--checksums", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            print(prepare(args.archive, args.checksums, args.output, args.tag,
                          args.source_sha, args.run_url))
        elif args.command == "import":
            import_port(args.prepared, args.repository)
        else:
            update_checksum(args.archive, args.checksums)
    except (ValueError, OSError, zipfile.BadZipFile, ET.ParseError,
            subprocess.CalledProcessError) as error:
        parser.exit(1, f"PortMaster preparation failed: {error}\n")


if __name__ == "__main__":
    main()
