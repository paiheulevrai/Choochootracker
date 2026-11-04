import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(sys.platform == "darwin", "Requires native macOS and SDL2.framework")
class MacOSWorkspaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="cct-macos-tests-")
        cls.addClassCleanup(cls.build.cleanup)
        cls.binary = Path(cls.build.name) / "assets-test"
        framework_parent = os.environ.get(
            "SDL2_FRAMEWORK_PARENT", str(ROOT / "tracker/platforms/macos/Frameworks")
        )
        subprocess.run([
            "clang++", "-std=c++17", "-DMACOS_BUILD",
            "-I" + str(ROOT / "tracker/src/corelib"),
            "-F" + framework_parent, "-Wl,-rpath," + framework_parent,
            str(ROOT / "tracker/tests/macos_assets.cpp"),
            str(ROOT / "tracker/platforms/sdl2/corelib_assets.cpp"),
            str(ROOT / "tracker/src/corelib/corelib_file.cpp"),
            "-framework", "SDL2", "-o", str(cls.binary),
        ], check=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cct test with spaces ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.home = self.root / "home"
        self.home.mkdir()
        self.bundle = self.root / "ChooChooTracker.app"
        contents = self.bundle / "Contents"
        (contents / "MacOS").mkdir(parents=True)
        self.executable = contents / "MacOS/assets-test"
        shutil.copy2(self.binary, self.executable)
        with (contents / "Info.plist").open("wb") as file:
            plistlib.dump({
                "CFBundleExecutable": "assets-test",
                "CFBundleIdentifier": "org.choochootracker.assets-test",
                "CFBundlePackageType": "APPL",
            }, file)
        self.common = contents / "Resources/common"
        (self.common / "projects").mkdir(parents=True)
        (self.common / "projects/example.cct").write_bytes(b"bundled project")
        (self.common / "themes").mkdir()
        (self.common / "themes/example.cth").write_bytes(b"bundled theme")
        self.workspace = self.home / "Library/Application Support/ChooChooTracker"
        self.cwd = self.root / "unrelated cwd"
        self.cwd.mkdir()

    def run_app(self, home=None):
        env = dict(os.environ, HOME=str(self.home if home is None else home))
        return subprocess.run(
            [str(self.executable)], cwd=self.cwd, env=env, capture_output=True, text=True
        )

    def test_first_launch_and_restart_never_overwrite_existing_files(self):
        result = self.run_app()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"cwd={self.workspace}", result.stdout)
        project = self.workspace / "projects/example.cct"
        self.assertEqual(project.read_bytes(), b"bundled project")
        project.write_bytes(b"user edits")
        (self.workspace / "settings.txt").write_bytes(b"user settings")
        (self.workspace / "autosave.cct").write_bytes(b"user autosave")
        (self.workspace / "themes/example.cth").unlink()
        result = self.run_app()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(project.read_bytes(), b"user edits")
        self.assertEqual((self.workspace / "settings.txt").read_bytes(), b"user settings")
        self.assertEqual((self.workspace / "autosave.cct").read_bytes(), b"user autosave")
        self.assertEqual((self.workspace / "themes/example.cth").read_bytes(), b"bundled theme")
        self.assertEqual(list(self.cwd.iterdir()), [])

    def test_bundle_does_not_need_write_access(self):
        paths = list(self.bundle.rglob("*")) + [self.bundle]
        modes = {path: path.stat().st_mode for path in paths}
        try:
            for path in paths:
                path.chmod(modes[path] & ~0o222)
            result = self.run_app()
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((self.workspace / "projects/example.cct").is_file())
        finally:
            for path in paths:
                path.chmod(modes[path])

    def test_invalid_home_is_an_explicit_failure(self):
        for home in ("", "relative-home"):
            result = self.run_app(home)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("macOS workspace", result.stderr)

    def test_blocked_workspace_is_an_explicit_failure(self):
        (self.home / "Library").write_text("not a directory")
        result = self.run_app()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot create macOS workspace", result.stderr)

    def test_readonly_home_is_an_explicit_failure(self):
        self.home.chmod(0o500)
        try:
            result = self.run_app()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Cannot create macOS workspace", result.stderr)
        finally:
            self.home.chmod(0o700)

    def test_conflicting_resource_is_an_explicit_failure(self):
        self.workspace.mkdir(parents=True)
        (self.workspace / "projects").write_text("not a directory")
        result = self.run_app()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot seed macOS resources", result.stderr)

    def test_missing_resources_are_an_explicit_failure(self):
        shutil.rmtree(self.common)
        result = self.run_app()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot seed macOS resources", result.stderr)


if __name__ == "__main__":
    unittest.main()
