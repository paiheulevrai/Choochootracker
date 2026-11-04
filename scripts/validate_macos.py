"""Validate the extracted macOS release, then smoke-test it on the native runner."""

import argparse
import filecmp
import os
from pathlib import Path
import plistlib
import re
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]


def check_macho(binary, frameworks, executable_dir):
    architectures = subprocess.check_output(["lipo", "-archs", str(binary)], text=True).split()
    if set(architectures) != {"arm64", "x86_64"}:
        raise ValueError(f"Not a universal arm64/x86_64 binary: {binary}: {architectures}")
    for arch in architectures:
        build = subprocess.check_output(
            ["vtool", "-arch", arch, "-show-build", str(binary)], text=True
        )
        minimum = re.search(r"^\s*(?:minos|version)\s+(\d+)\.(\d+)", build, re.MULTILINE)
        if not minimum or tuple(map(int, minimum.groups())) > (11, 0):
            raise ValueError(f"Invalid macOS minimum for {arch}: {binary}\n{build}")
        linked = subprocess.check_output(
            ["otool", "-arch", arch, "-L", str(binary)], text=True
        )
        for line in linked.splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            if dependency.startswith("@rpath/"):
                resolved = frameworks / dependency.removeprefix("@rpath/")
            elif dependency.startswith("@executable_path/"):
                resolved = executable_dir / dependency.removeprefix("@executable_path/")
            else:
                raise ValueError(f"Non-portable dependency in {binary}: {dependency}")
            if not resolved.is_file():
                raise ValueError(f"Missing bundled dependency: {resolved}")


def smoke_test(bundle):
    logs = ROOT / "tracker/build/macos-validation"
    logs.mkdir(parents=True, exist_ok=True)
    binary = bundle / "Contents/MacOS/choochootracker"
    with tempfile.TemporaryDirectory(prefix="cct smoke with spaces ") as temp:
        root = Path(temp).resolve()
        home = root / "home"
        home.mkdir()
        workspace = home / "Library/Application Support/ChooChooTracker"
        env = dict(os.environ, HOME=str(home), SDL_VIDEODRIVER="dummy",
                   SDL_AUDIODRIVER="dummy", CCT_STARTUP_TIMING="1")
        for attempt in range(2):
            log_path = logs / f"startup-{attempt}.log"
            with log_path.open("w") as log:
                process = subprocess.Popen([str(binary)], cwd=root, env=env,
                                           stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 90
                    while time.monotonic() < deadline:
                        if process.poll() is not None:
                            raise RuntimeError(f"Application exited during startup:\n{log_path.read_text()}")
                        if "[startup] first title present:" in log_path.read_text():
                            break
                        time.sleep(0.2)
                    else:
                        raise RuntimeError(f"Startup timed out:\n{log_path.read_text()}")
                    time.sleep(1)
                    if process.poll() is not None:
                        raise RuntimeError(f"Application crashed:\n{log_path.read_text()}")
                finally:
                    if process.poll() is None:
                        process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                        raise RuntimeError("Application did not terminate after smoke test")
            for source in (bundle / "Contents/Resources/common").rglob("*"):
                if source.is_file():
                    destination = workspace / source.relative_to(bundle / "Contents/Resources/common")
                    if not destination.is_file():
                        raise ValueError(f"Resource not seeded: {destination}")
            sentinel = workspace / "instruments/USER/dx7/README.txt"
            if attempt == 0:
                sentinel.write_text("Preserve user edits\n")
                (workspace / "themes/Bb22200.cth").unlink()
            elif sentinel.read_text() != "Preserve user edits\n":
                raise ValueError("Second launch overwrote a user file")
        if not (workspace / "themes/Bb22200.cth").is_file():
            raise ValueError("Second launch did not restore a missing bundled resource")
        if (root / "settings.txt").exists() or (root / "projects").exists():
            raise ValueError("Application wrote into the caller's directory")


def validate(bundle):
    contents = bundle / "Contents"
    subprocess.run(["plutil", "-lint", str(contents / "Info.plist")], check=True)
    with (contents / "Info.plist").open("rb") as file:
        info = plistlib.load(file)
    expected = {
        "CFBundleExecutable": "choochootracker",
        "CFBundleIdentifier": "org.choochootracker.ChooChooTracker",
        "CFBundleName": "ChooChooTracker",
        "CFBundleDisplayName": "ChooChooTracker",
        "LSMinimumSystemVersion": "11.0",
    }
    for key, value in expected.items():
        if info.get(key) != value:
            raise ValueError(f"{key}: expected {value}, got {info.get(key)}")
    for key in ("CFBundleVersion", "CFBundleShortVersionString"):
        if not re.fullmatch(r"\d+\.\d+\.\d+", info[key]):
            raise ValueError(f"Invalid Apple version: {key}: {info[key]}")
    if not info.get("NSMicrophoneUsageDescription"):
        raise ValueError("Missing microphone permission description")
    for source in (ROOT / "tracker/packaging/common").rglob("*"):
        if source.is_file():
            destination = contents / "Resources/common" / source.relative_to(ROOT / "tracker/packaging/common")
            if not destination.is_file() or not filecmp.cmp(source, destination, shallow=False):
                raise ValueError(f"Missing or modified bundled resource: {destination}")
    for relative in (
        "Resources/ChooChooTracker.icns", "Resources/USER_MANUAL.md",
        "Resources/licenses/LICENSE", "Frameworks/SDL2.framework/SDL2",
    ):
        if not (contents / relative).is_file():
            raise ValueError(f"Missing bundle file: {relative}")
    frameworks = contents / "Frameworks"
    for binary in (contents / "MacOS/choochootracker", frameworks / "SDL2.framework/SDL2"):
        check_macho(binary, frameworks, contents / "MacOS")
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
    smoke_test(bundle)
    print("macOS universal bundle and native startup validated.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    validate(args.bundle.resolve())
