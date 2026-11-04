import os
from pathlib import Path
import subprocess
import tempfile
import unittest


LAUNCHER = Path(__file__).resolve().parents[1] / "packaging" / "portmaster" / "ChooChooTracker.sh"


class PortMasterLauncherTest(unittest.TestCase):
    def run_launcher(self, firmware):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            control = root / "PortMaster"
            control.mkdir()
            game = root / "ports" / "choochootracker"
            game.mkdir(parents=True)
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
            )
            env = dict(os.environ, XDG_DATA_HOME=tmp, HOME=tmp,
                       CCT_ROOT=tmp, CCT_CALLS=str(calls), CCT_FIRMWARE=firmware)
            # Start at the shared bootstrap to isolate host-specific discovery paths.
            script = LAUNCHER.read_text().split('source "$controlfolder/control.txt"', 1)[1]
            result = subprocess.run(
                ["bash", "-c",
                 'controlfolder="$XDG_DATA_HOME/PortMaster"\n'
                 'source "$controlfolder/control.txt"\n' + script],
                env=env, capture_output=True, text=True, timeout=10,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
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


if __name__ == "__main__":
    unittest.main()
