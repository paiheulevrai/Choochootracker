---
name: portmaster-update
description: Prepare and submit a ChooChooTracker update to the PortMaster Library while preserving the accepted port and user data.
---

# PortMaster Library update

Use this for Library submissions, not just for building the standalone release.
The canonical build, artifact preparation, and import commands are in
`docs/build-notes.md`, under "PortMaster Library submission and weekly updates".

1. Inspect the accepted port and its last merged PR, including reviewer-requested
   changes, current Library contribution requirements, and any open update PR.
   Work in a separate sparse clone and branch; do not overwrite unrelated work.
2. Select the intended release and verify the tag's source commit, the build
   run's source commit, the artifact's provenance, and the release ZIP's
   checksum. Do not assume a release asset was built from the current tag:
   an asset may predate a moved tag or be rebuilt from another revision. If
   they differ, use the intended verified artifact and disclose the difference.
3. Import the verified Library artifact with
   `scripts/prepare_portmaster_library.py import`. Compare the result against
   the accepted port, not merely the generated template. Preserve accepted
   launcher, metadata, licensing and source-bank adaptations unless the update
   specifically requires changing them. For example, do not replace the
   accepted non-destructive `data.tar.gz` extraction with an older launcher.
4. Review the complete diff and package contents, validate the archive and
   executable architecture, and run the focused `test_portmaster_*.py` tests.
   On Windows, use WSL for tests that exercise the Bash launcher. Also simulate
   an upgrade from the previous release with existing and modified user files:
   verify they survive, new assets appear, and the archive is removed only
   after successful extraction. Verify failure retains the archive for retry.
5. Submit only the intended port files, following the accepted PR's format and
   current checklist. Record the release, source SHA, artifact run and checksum
   in the PR. Report only checks actually performed; prior hardware results do
   not certify a new build, and private testing links must not be invented.
   Mark console testing as outstanding until a person tests that build.
