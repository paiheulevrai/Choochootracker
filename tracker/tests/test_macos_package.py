import importlib.util
from pathlib import Path
import plistlib
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("validate_macos", ROOT / "scripts/validate_macos.py")
macos = importlib.util.module_from_spec(spec)
spec.loader.exec_module(macos)


class MacOSPackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.frameworks = self.root / "Frameworks"
        self.library = self.frameworks / "SDL2.framework/Versions/A/SDL2"
        self.library.parent.mkdir(parents=True)
        self.library.touch()
        self.architectures = "arm64 x86_64"
        self.minimum = "11.0"
        self.dependency = "@rpath/SDL2.framework/Versions/A/SDL2"

    def tool_output(self, command, **kwargs):
        if command[0] == "lipo":
            return self.architectures
        if command[0] == "vtool":
            return f"Load command 1\n    cmd LC_BUILD_VERSION\n    minos {self.minimum}\n"
        if command[0] == "otool":
            return f"binary:\n\t{self.dependency} (compatibility version 1.0.0)\n"
        self.fail(f"Unexpected command: {command}")

    def check_binary(self):
        with patch.object(macos.subprocess, "check_output", side_effect=self.tool_output):
            macos.check_macho(self.root / "binary", self.frameworks, self.root / "MacOS")

    def test_universal_binary_with_bundled_dependency_and_macos_11_is_accepted(self):
        self.check_binary()

    def test_newer_minimum_os_is_rejected(self):
        self.minimum = "11.1"
        with self.assertRaisesRegex(ValueError, "Invalid macOS minimum"):
            self.check_binary()

    def test_missing_minimum_os_is_rejected(self):
        self.minimum = "unknown"
        with self.assertRaisesRegex(ValueError, "Invalid macOS minimum"):
            self.check_binary()

    def test_single_architecture_is_rejected(self):
        self.architectures = "arm64"
        with self.assertRaisesRegex(ValueError, "Not a universal"):
            self.check_binary()

    def test_external_homebrew_dependency_is_rejected(self):
        self.dependency = "/opt/homebrew/lib/libSDL2.dylib"
        with self.assertRaisesRegex(ValueError, "Non-portable dependency"):
            self.check_binary()

    def test_missing_bundled_dependency_is_rejected(self):
        self.library.unlink()
        with self.assertRaisesRegex(ValueError, "Missing bundled dependency"):
            self.check_binary()

    def test_system_framework_is_accepted(self):
        self.dependency = "/System/Library/Frameworks/CoreAudio.framework/Versions/A/CoreAudio"
        self.check_binary()

    def test_package_validation_checks_real_asset_contents_before_startup(self):
        common = self.root / "tracker/packaging/common"
        common.mkdir(parents=True)
        (common / "sample.wav").write_bytes(b"sample")
        bundle = self.root / "ChooChooTracker.app"
        resources = bundle / "Contents/Resources"
        (resources / "common").mkdir(parents=True)
        info = {
            "CFBundleExecutable": "choochootracker",
            "CFBundleIdentifier": "org.choochootracker.ChooChooTracker",
            "CFBundleName": "ChooChooTracker",
            "CFBundleDisplayName": "ChooChooTracker",
            "LSMinimumSystemVersion": "11.0",
            "CFBundleVersion": "1.0.23",
            "CFBundleShortVersionString": "1.0.23",
            "NSMicrophoneUsageDescription": "Record samples",
        }
        with (bundle / "Contents/Info.plist").open("wb") as file:
            plistlib.dump(info, file)
        destination = resources / "common/sample.wav"
        destination.write_bytes(b"broken")
        with patch.object(macos, "ROOT", self.root), \
                patch.object(macos.subprocess, "run"), \
                patch.object(macos, "smoke_test") as smoke:
            with self.assertRaisesRegex(ValueError, "Missing or modified bundled resource"):
                macos.validate(bundle)
            smoke.assert_not_called()


if __name__ == "__main__":
    unittest.main()
