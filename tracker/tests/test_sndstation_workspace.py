import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(sys.platform.startswith("linux"), "Requires Linux and SDL2")
class SNDStationWorkspaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="cct-sndstation-tests-")
        cls.addClassCleanup(cls.build.cleanup)
        cls.binary = Path(cls.build.name) / "assets-test"
        flags = subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True
        ).split()
        subprocess.run([
            "c++", "-std=c++17", "-DSNDSTATION_BUILD",
            "-I" + str(ROOT / "tracker/src/corelib"),
            str(ROOT / "tracker/tests/macos_assets.cpp"),
            str(ROOT / "tracker/platforms/sdl2/corelib_assets.cpp"),
            str(ROOT / "tracker/src/corelib/corelib_file.cpp"),
            "-o", str(cls.binary), *flags,
        ], check=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cct sndstation ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / "home"
        self.home.mkdir()
        executable = self.root / "usr/bin/assets-test"
        executable.parent.mkdir(parents=True)
        shutil.copy2(self.binary, executable)
        self.executable = executable
        self.common = self.root / "usr/share/choochootracker/common"
        (self.common / "projects").mkdir(parents=True)
        (self.common / "projects/example.cct").write_bytes(b"bundled project")
        (self.common / "themes").mkdir()
        (self.common / "themes/example.cth").write_bytes(b"bundled theme")
        self.workspace = self.home / ".local/share/ChooChooTracker"

    def run_app(self, **variables):
        env = dict(os.environ, HOME=str(self.home))
        env.pop("XDG_DATA_HOME", None)
        env.update(variables)
        return subprocess.run(
            [str(self.executable)], cwd=self.root, env=env, capture_output=True, text=True
        )

    def test_seed_and_restart_preserve_user_data(self):
        result = self.run_app()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"cwd={self.workspace}", result.stdout)
        project = self.workspace / "projects/example.cct"
        self.assertEqual(project.read_bytes(), b"bundled project")
        project.write_bytes(b"user edits")
        (self.workspace / "settings.txt").write_bytes(b"user settings")
        (self.workspace / "themes/example.cth").unlink()
        result = self.run_app()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(project.read_bytes(), b"user edits")
        self.assertEqual((self.workspace / "settings.txt").read_bytes(), b"user settings")
        self.assertEqual((self.workspace / "themes/example.cth").read_bytes(), b"bundled theme")

    def test_xdg_workspace_is_used(self):
        data = self.root / "data"
        result = self.run_app(XDG_DATA_HOME=str(data))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"cwd={data / 'ChooChooTracker'}", result.stdout)

    def test_legacy_workspace_can_run_without_packaged_assets(self):
        self.workspace.mkdir(parents=True)
        executable = self.workspace / "assets-test"
        shutil.copy2(self.binary, executable)
        (self.workspace / "settings.txt").write_bytes(b"user settings")
        self.executable = executable
        result = self.run_app()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"cwd={self.workspace}", result.stdout)
        self.assertEqual((self.workspace / "settings.txt").read_bytes(), b"user settings")

    def test_invalid_home_or_resources_fail(self):
        for variables in ({"HOME": ""}, {"XDG_DATA_HOME": "relative"}):
            result = self.run_app(**variables)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("SNDStation workspace", result.stderr)
        shutil.rmtree(self.common)
        result = self.run_app()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot seed SNDStation resources", result.stderr)

    def test_conflicting_user_file_fails_without_overwriting(self):
        self.workspace.mkdir(parents=True)
        conflict = self.workspace / "projects"
        conflict.write_bytes(b"user data")
        result = self.run_app()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot seed SNDStation resources", result.stderr)
        self.assertEqual(conflict.read_bytes(), b"user data")


if __name__ == "__main__":
    unittest.main()
