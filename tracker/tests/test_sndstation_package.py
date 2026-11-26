import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PACKAGING = ROOT / "tracker/packaging/sndstation"


@unittest.skipUnless(sys.platform.startswith("linux"), "Requires Linux packaging tools")
class SNDStationPackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cct-snd-package-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.tracker = self.root / "tracker"
        files = {
            "tracker/packaging/common/projects/example.cct": b"project",
            "tracker/packaging/portmaster/license/extra.txt": b"license",
            "docs/USER_MANUAL.md": b"manual",
            "LICENSE": b"license",
            "chipnomad_lib/external/insert_fx/LICENSE-JM-Formant": b"license",
        }
        for name, content in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        subprocess.run(
            ["bash", str(PACKAGING / "package-dev.sh"), "/bin/true", "1.2.3-alpha"],
            cwd=self.tracker, check=True, capture_output=True, text=True,
        )
        self.deb = self.tracker / "build/sndstation/choochootracker-sndstation_1.2.3~alpha_arm64.deb"

    def test_package_metadata_and_content(self):
        fields = subprocess.check_output(
            ["dpkg-deb", "-f", str(self.deb)], text=True
        )
        self.assertIn("Architecture: arm64\n", fields)
        self.assertIn("Sndstation-Kind: app\n", fields)
        extracted = self.root / "extracted"
        subprocess.run(["dpkg-deb", "-x", str(self.deb), str(extracted)], check=True)
        manifest = json.loads(
            (extracted / "usr/share/sndstation/apps/choochootracker.json").read_text()
        )
        self.assertEqual(manifest["exec"], ["/usr/bin/choochootracker-sndstation"])
        self.assertTrue((extracted / manifest["exec"][0].lstrip("/")).is_file())
        software_manifest = json.loads(
            (extracted / "usr/share/sndstation/apps/choochootracker-software.json").read_text()
        )
        self.assertEqual(
            software_manifest["exec"], ["/usr/bin/choochootracker-sndstation-software"]
        )
        launcher = extracted / software_manifest["exec"][0].lstrip("/")
        self.assertTrue(os.access(launcher, os.X_OK))
        self.assertEqual(
            launcher.read_bytes(),
            b'#!/bin/sh\nexport SDL_RENDER_DRIVER=software\n'
            b'exec /usr/bin/choochootracker-sndstation "$@"\n',
        )
        self.assertEqual(
            (extracted / "usr/share/choochootracker/common/projects/example.cct").read_bytes(),
            b"project",
        )

    def test_local_repository_is_signed_and_does_not_replace_existing_directory(self):
        home = self.root / "gnupg"
        home.mkdir(mode=0o700)
        env = dict(os.environ, GNUPGHOME=str(home), REPO_KEY="cct-test@example.invalid")
        self.addCleanup(subprocess.run, ["gpgconf", "--kill", "gpg-agent"],
                        env=env, check=False, capture_output=True)
        subprocess.run(
            ["gpg", "--batch", "--passphrase", "", "--quick-generate-key",
             env["REPO_KEY"], "ed25519", "sign", "1d"],
            env=env, check=True, capture_output=True, text=True,
        )
        repository = self.root / "repository"
        command = ["bash", str(PACKAGING / "repo-dev.sh"), str(self.deb), str(repository)]
        subprocess.run(command, env=env, check=True, capture_output=True, text=True)
        release_dir = repository / "dists/stable"
        subprocess.run(["gpg", "--verify", str(release_dir / "InRelease")],
                       env=env, check=True, capture_output=True, text=True)
        subprocess.run(["gpg", "--verify", str(release_dir / "Release.gpg"),
                        str(release_dir / "Release")],
                       env=env, check=True, capture_output=True, text=True)
        packages = (release_dir / "main/binary-arm64/Packages").read_bytes()
        self.assertIn(hashlib.sha256(packages).hexdigest(),
                      (release_dir / "Release").read_text())
        self.assertIn(hashlib.sha256(self.deb.read_bytes()).hexdigest(), packages.decode())
        self.assertIn("Sndstation-Kind: app", packages.decode())
        keyring = self.root / "vendor.gpg"
        with keyring.open("wb") as file:
            subprocess.run(["gpg", "--export", env["REPO_KEY"]], env=env,
                           check=True, stdout=file)
        sources = self.root / "sources.list"
        sources.write_text(f"deb [arch=arm64 signed-by={keyring}] file:{repository} stable main\n")
        lists = self.root / "lists"
        lists.mkdir()
        subprocess.run(
            ["apt-get", "-o", f"Dir::Etc::sourcelist={sources}",
             "-o", "Dir::Etc::sourceparts=-", "-o", f"Dir::State::lists={lists}",
             "-o", "Dir::State::status=/dev/null", "-o", "APT::Architecture=arm64",
             "-o", "Debug::NoLocking=1", "update"],
            env=env, check=True, capture_output=True, text=True,
        )
        self.assertTrue(any("Packages" in path.name for path in lists.iterdir()))
        result = subprocess.run(command, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not already exist", result.stderr)


if __name__ == "__main__":
    unittest.main()
