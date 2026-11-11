import os
import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import tarfile
import unittest


LAUNCHER = Path(__file__).resolve().parents[1] / "packaging" / "portmaster" / "ChooChooTracker.sh"
DEBUG_LAUNCHER = LAUNCHER.with_name("ChooChooTracker-debug.sh")


class PortMasterLauncherTest(unittest.TestCase):
    def run_launcher(self, firmware, assets=None, installed=True, corrupt=False,
                     launches=1, blocked=False, retry=False):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            control = root / "PortMaster"
            control.mkdir()
            game = root / "ports" / "choochootracker"
            game.mkdir(parents=True)
            defaults = {"instruments/FACTORY/catalog.tsv": b"catalog",
                        "instruments/FACTORY/builtins.tsv": b"builtins"}
            if installed:
                for name, data in {**defaults, "projects/My song.cct": b"user song"}.items():
                    target = game / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
            archive = game / "data.tar.gz"
            if assets is not None:
                with tarfile.open(archive, "w:gz") as package:
                    for name, data in {**defaults, **assets}.items():
                        info = tarfile.TarInfo(name)
                        info.size = len(data)
                        package.addfile(info, io.BytesIO(data))
            if corrupt:
                archive.write_bytes(archive.read_bytes()[:-8] if corrupt == "truncated"
                                    else b"broken archive")
            if blocked:
                (game / "samples").write_bytes(b"user file blocking a directory")
            calls = root / "calls"
            mapper = root / "mapper"
            mapper.write_text(
                '#!/bin/bash\nprintf "%s %s %s\\n" "$CCT_MAPPER" "$HOTKEY" "$*" >> "$CCT_CALLS"\n'
            )
            mapper.chmod(0o755)
            binary = game / "choochootracker.aarch64"
            binary.write_text('#!/bin/bash\nprintf "app\\n" >> "$CCT_CALLS"\n')
            binary.chmod(0o644)
            (control / "control.txt").write_text(
                'directory="${CCT_ROOT#/}"\n'
                'DEVICE_ARCH=aarch64\n'
                'CFW_NAME="$CCT_FIRMWARE"\n'
                'GPTOKEYB="env CCT_MAPPER=old $CCT_ROOT/mapper -1"\n'
                'GPTOKEYB2="env CCT_MAPPER=new $CCT_ROOT/mapper -Z"\n'
                'get_controls() { :; }\n'
                'bind_directories() { :; }\n'
                'pm_platform_helper() { wait "$!"; }\n'
                'pm_finish() { :; }\n'
                'pm_message() { printf "%s\\n" "$*"; }\n'
            )
            env = dict(os.environ, XDG_DATA_HOME=tmp, HOME=tmp,
                       CCT_ROOT=tmp, CCT_CALLS=str(calls), CCT_FIRMWARE=firmware)
            # Start at the shared bootstrap to isolate host-specific discovery paths.
            script = LAUNCHER.read_text().split('source "$controlfolder/control.txt"', 1)[1]
            for launch in range(launches):
                result = subprocess.run(
                    ["bash", "-c",
                     'controlfolder="$XDG_DATA_HOME/PortMaster"\n'
                     'source "$controlfolder/control.txt"\n' + script],
                    env=env, capture_output=True, text=True, timeout=10,
                )
                if corrupt or (blocked and launch == 0) or (not installed and assets is None):
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Error:", result.stdout)
                    self.assertFalse(calls.exists())
                    self.assertEqual(archive.exists(), bool(corrupt or blocked))
                    if corrupt:
                        self.assertFalse((game / "projects/New song.cct").exists())
                    self.assertFalse(list(game.glob(".data-extract.*")))
                    if retry:
                        (game / "samples").unlink()
                        continue
                    return []
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertFalse(archive.exists())
                self.assertFalse(list(game.glob(".data-extract.*")))
                for name, data in (assets or {}).items():
                    expected = b"user song" if installed and name == "projects/My song.cct" else data
                    self.assertEqual((game / name).read_bytes(), expected)
            lines = calls.read_text().splitlines()
            self.assertIn("app", lines)
            return [line for line in lines if line != "app"]

    def test_rocknix_uses_new_mapper_with_existing_controls(self):
        self.assertEqual(self.run_launcher("ROCKNIX"), [
            "new guide -Z choochootracker.aarch64 -c ./choochootracker.gptk"
        ])

    def test_other_firmware_keeps_existing_mapper(self):
        for firmware in ("ArkOS", "JELOS", ""):
            with self.subTest(firmware=firmware):
                self.assertEqual(self.run_launcher(firmware), [
                    "old guide -1 choochootracker.aarch64 -c ./choochootracker.gptk"
                ])

    def test_first_run_extracts_data_and_second_run_needs_no_archive(self):
        self.run_launcher("ROCKNIX", installed=False, launches=2,
                          assets={"projects/New song.cct": b"factory song",
                                  "fonts/Pixel/font.bin": b"font"})

    def test_update_preserves_existing_projects_and_adds_missing_assets(self):
        self.run_launcher("ArkOS", assets={"projects/My song.cct": b"factory song",
                                          "samples/New sample.wav": b"sample"})

    def test_corrupt_archive_stops_before_starting_app_and_is_retained(self):
        self.run_launcher("ROCKNIX", installed=False, corrupt=True)

    def test_truncated_archive_does_not_install_partial_assets(self):
        self.run_launcher("ROCKNIX", installed=False, corrupt="truncated",
                          assets={"projects/New song.cct": b"factory song"})

    def test_missing_archive_and_data_stops_before_starting_app(self):
        self.run_launcher("ROCKNIX", installed=False)

    def test_failed_merge_retains_archive_and_retry_completes(self):
        self.run_launcher("ROCKNIX", blocked=True, retry=True, launches=2,
                          assets={"projects/New song.cct": b"complete song",
                                  "samples/New sample.wav": b"sample"})

    def test_debug_launcher_traces_original_and_writes_diagnostic_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "ChooChooTracker.sh").write_text(
                '#!/bin/bash\nprintf "stub launcher ran\\n"\n'
            )
            shutil.copyfile(DEBUG_LAUNCHER, root / DEBUG_LAUNCHER.name)
            (root / "choochootracker").mkdir()
            result = subprocess.run(
                ["bash", DEBUG_LAUNCHER.name],
                env=dict(os.environ, HOME=tmp, XDG_DATA_HOME=tmp),
                capture_output=True, text=True, timeout=10,
                cwd=root,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            log = (root / "cct-startup.txt").read_text()
            self.assertIn("=== Original launcher trace ===", log)
            self.assertIn("stub launcher ran", log)
            self.assertIn("=== Launcher exited with status 0 ===", log)


if __name__ == "__main__":
    unittest.main()
