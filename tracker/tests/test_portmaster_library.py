import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import prepare_portmaster_library as library


class LibraryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.archive = self.root / "ChooChooTracker-v1-test-PortMaster.zip"
        self.manifest = self.root / "SHA256SUMS.txt"
        self.output = self.root / "prepared"
        self.files = {
            "ChooChooTracker.sh": b"#!/bin/bash\r\npm_finish\r\n",
            "choochootracker/choochootracker.aarch64": b"\x7fELF\x02\x01" + b"\0" * 12 + b"\xb7\0",
            "choochootracker/choochootracker.gptk": b"a = x\r\n",
            "choochootracker/USER_MANUAL.md": b"manual",
            "choochootracker/port.json": json.dumps({
                "version": 4, "name": "choochootracker.zip",
                "items": ["ChooChooTracker.sh", "choochootracker"],
                "attr": {"arch": ["aarch64"]},
            }).encode(),
            "choochootracker/README.md": b"credits",
            "choochootracker/gameinfo.xml": (
                b"<gameList><game><path>./ChooChooTracker.sh</path>"
                b"<image>./choochootracker/cover.png</image>"
                b"<releasedate>20261104T120000</releasedate></game></gameList>"
            ),
            "choochootracker/screenshot.png": (
                b"\x89PNG\r\n\x1a\n" + b"\0" * 8 + (640).to_bytes(4, "big")
                + (480).to_bytes(4, "big")
            ),
            "choochootracker/cover.png": b"cover",
            "choochootracker/licenses/engine/LICENSE": b"engine license",
            "choochootracker/fonts/Pixel/licenses/font.txt": b"font license",
            "choochootracker/instruments/FACTORY/catalog.tsv": b"catalog",
            "choochootracker/instruments/FACTORY/builtins.tsv": b"builtins",
            "choochootracker/instruments/FACTORY/banks.zip": b"bank",
            "choochootracker/projects/DNB.cct": b"song",
        }

    def write_package(self):
        with zipfile.ZipFile(self.archive, "w") as archive:
            for name, data in self.files.items():
                info = zipfile.ZipInfo(name)
                info.filename = name
                archive.writestr(info, data)
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(f"{digest}  {self.archive.name}\n", encoding="utf-8")

    def prepare(self):
        self.write_package()
        return library.prepare(self.archive, self.manifest, self.output,
                               "v1-test", "a" * 40, "https://github.com/example/run/1")

    def repository(self):
        repo = self.root / "PortMaster-New"
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        subprocess.run(["git", "-C", str(repo), "config", "core.autocrlf", "false"], check=True)
        (repo / "SOURCE_SETUP.txt").write_text("PortMaster")
        return repo

    def test_exact_layout_preserves_runtime_and_flattens_licenses(self):
        port = self.prepare()
        self.assertEqual({p.name for p in port.iterdir()}, {
            *library.METADATA, "ChooChooTracker.sh", "choochootracker",
        })
        runtime = port / "choochootracker"
        self.assertEqual((runtime / "projects" / "DNB.cct").read_bytes(), b"song")
        self.assertEqual((runtime / "instruments/FACTORY/banks.zip").read_bytes(), b"bank")
        licenses = list((runtime / "licenses").iterdir())
        self.assertEqual({p.read_bytes() for p in licenses},
                         {b"engine license", b"font license"})
        self.assertTrue(all(p.is_file() for p in licenses))
        self.assertFalse((runtime / "fonts/Pixel/licenses").exists())
        self.assertNotIn(b"\r", (port / "ChooChooTracker.sh").read_bytes())
        self.assertNotIn(b"\r", (runtime / "choochootracker.gptk").read_bytes())
        provenance = json.loads((self.output / "provenance.json").read_text())
        self.assertEqual(provenance["source_sha"], "a" * 40)
        self.assertIn("not certification", (self.output / "PR_BODY.md").read_text())
        self.assertIn("https://discord.com/channels/1122861252088172575/1561823001669013595",
                      (self.output / "PR_BODY.md").read_text())

    def test_checksum_mismatch_leaves_no_output(self):
        self.write_package()
        self.archive.write_bytes(self.archive.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "Checksum mismatch"):
            library.prepare(self.archive, self.manifest, self.output, "v1", "sha", "run")
        self.assertFalse(self.output.exists())

    def test_rejects_missing_files_and_wrong_architecture(self):
        for name, replacement in [
            ("choochootracker/USER_MANUAL.md", b""),
            ("choochootracker/choochootracker.aarch64", b"x86"),
            ("choochootracker/screenshot.png", b"not png"),
            ("choochootracker/gameinfo.xml", b"<gameList/>"),
        ]:
            original = self.files[name]
            with self.subTest(name=name):
                self.files[name] = replacement
                with self.assertRaises(ValueError):
                    self.prepare()
                self.assertFalse(self.output.exists())
            self.files[name] = original

    def test_rejects_unsafe_paths_and_system_libraries(self):
        for name in ("../escape", "/absolute", "choochootracker/../escape",
                     "choochootracker\\escape", "choochootracker/libs/libSDL2.so"):
            with self.subTest(name=name):
                self.files[name] = b"bad"
                with self.assertRaises(ValueError):
                    self.prepare()
                self.assertFalse(self.output.exists())
                del self.files[name]

    def test_does_not_overwrite_prepared_output(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "Output must be empty"):
            self.prepare()

    def test_updates_only_exact_checksum_entry(self):
        self.write_package()
        other = b"1" * 64 + b"  Other.zip\r\n"
        self.manifest.write_bytes(other + b"0" * 64 + b"  " +
                                  self.archive.name.encode() + b"\r\n")
        library.update_checksum(self.archive, self.manifest)
        expected = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.assertEqual(self.manifest.read_bytes(),
                         other + expected.encode() + b"  " +
                         self.archive.name.encode() + b"\r\n")

    def test_requires_existing_unique_checksum(self):
        self.write_package()
        entry = self.manifest.read_text()
        for contents in ("0" * 64 + "  Other.zip\n", entry + entry, "invalid\n"):
            with self.subTest(contents=contents):
                self.manifest.write_text(contents)
                with self.assertRaises(ValueError):
                    library.update_checksum(self.archive, self.manifest)
                self.assertEqual(self.manifest.read_text(), contents)

    def test_import_refuses_local_changes(self):
        self.prepare()
        repo = self.repository()
        library.import_port(self.output, repo)
        port = repo / "ports/choochootracker"
        (port / "local-project.cct").write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError, "local changes"):
            library.import_port(self.output, repo)
        self.assertEqual((port / "local-project.cct").read_bytes(), b"keep")

    def test_import_replaces_only_a_clean_port(self):
        self.prepare()
        repo = self.repository()
        library.import_port(self.output, repo)
        subprocess.run(["git", "-C", str(repo), "add", "."], check=True)
        subprocess.run(["git", "-C", str(repo), "-c", "user.name=Test",
                        "-c", "user.email=test@example.invalid",
                        "commit", "-qm", "Initial"], check=True)
        outside = repo / "unrelated.txt"
        outside.write_bytes(b"keep")
        library.import_port(self.output, repo)
        self.assertEqual(outside.read_bytes(), b"keep")
        self.assertEqual(subprocess.check_output(
            ["git", "-C", str(repo), "status", "--porcelain", "--",
             "ports/choochootracker"]), b"")


if __name__ == "__main__":
    unittest.main()
