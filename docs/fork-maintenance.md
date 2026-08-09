# Maintaining the ChooChooTracker fork

ChooChooTracker started from ChipNomad, but the projects now have different goals. ChipNomad is moving much of its code into C++ classes and rebuilding its UI engine. ChooChooTracker has Braids, Plaits, a PCM sampler, Reverb and Delay sends, instrument-specific FX, and a defined PortMaster hardware target.

Trying to keep both repositories identical would create frequent conflicts and tie ChooChooTracker's architecture to decisions made for another project. ChipNomad remains a useful source of fixes and ideas. ChooChooTracker owns its code, project format, and release schedule.

## Ownership boundaries

| Area | Policy |
| --- | --- |
| `chipnomad_lib/external/mutable/` | Frozen snapshots at the documented commits. Do not update them on a schedule. |
| `chipnomad_lib/synth/BraidsVoice` | ChooChooTracker code and the boundary between Braids and the rest of the engine. |
| Audio engine, instruments, and `.cct` format | ChooChooTracker code, including areas that began in ChipNomad. |
| UI and sequencer | Review ChipNomad changes and take only the ones that help this project. |
| PCM sampler, sends, and Braids/Plaits/Sample FX | ChooChooTracker code. Keep the existing FX lanes and the term "FX." |
| SDL2, input, file handling, and build targets | Good candidates for upstream fixes after Windows and PortMaster testing. |
| PortMaster packaging | ChooChooTracker code. Current PortMaster rules take priority over historical ChipNomad conventions. |

These boundaries keep stable Mutable DSP separate from tracker code that is still changing.

## Configure the upstream remote

The repository uses `origin` for the public ChooChooTracker repository and `upstream` for ChipNomad.

```sh
git remote add upstream https://github.com/Megus/chipnomad-tracker.git
git fetch upstream
git remote -v
```

If `upstream` already exists, run `git fetch upstream`. Do not merge `upstream/main` into `main` by habit.

Record the starting ChipNomad commit in the sync log at the end of this document. If a local commit exactly matches that base, it can also receive a tag:

```sh
git tag chipnomad-base-YYYYMMDD <local-commit>
```

Do not invent a base tag if the initial import already contains ChooChooTracker changes. The upstream SHA in the log is enough.

## Review ChipNomad changes

A monthly review, or one before a release, is enough. Following every upstream commit in real time adds no value here.

```sh
git fetch upstream
git log --oneline --decorate <last-upstream-sha>..upstream/main
git diff --stat <last-upstream-sha>..upstream/main
```

Classify changes before writing code:

1. Review crash, project corruption, audio, and portability fixes immediately.
2. Study project format changes even when they will not be adopted, because they may explain future incompatibilities.
3. Reimplement UI improvements when they suit ChooChooTracker's small screen and controls.
4. Do not import C++ rewrites just to resemble upstream. They must solve a problem that exists here.
5. Ignore renames, file moves, and cleanup with no user-visible effect unless they make a later fix easier to adopt.

Skipping a commit is not technical debt. It often solves a constraint that only exists in ChipNomad.

## Import a fix

Use a short-lived branch created from a clean, working `main` for each sync batch.

```sh
git switch main
git switch -c sync/chipnomad-YYYYMMDD
git cherry-pick -x <upstream-sha>
```

The `-x` option records the source commit in the message. Cherry-pick a small fix when it applies cleanly.

If upstream rewrote the code around new classes and cherry-picking would pull in the whole architecture, reimplement only the corrected behavior. Cite the upstream SHA in the local commit and explain why it was adapted.

Example:

```text
fix: preserve empty project titles when loading

Adapted from ChipNomad <sha>. The upstream implementation depends on the new
Project class, so this keeps the fix in ChooChooTracker's current parser.
```

Do not mix an upstream fix and a new ChooChooTracker feature in one sync. Separate changes are easier to test and revert.

## Resolve conflicts

Conflict resolution must preserve ChooChooTracker behavior, not reproduce the current shape of ChipNomad code.

Review these areas manually:

- allocation and destruction of `ChipNomadState`, `BraidsVoice`, and other C++ voice objects
- the SDL audio thread and data shared with the UI
- routing between AY, Braids, Plaits, and Sample instruments
- project save and load
- keyboard and controller event conversion
- data paths on Windows, Linux, and PortMaster

Do not accept an entire side automatically in these files. Read the full flow and its callers, then make the smallest change that preserves both required behaviors.

If an upstream fix would require several days of C++ migration, close the sync branch and create a separate task. A sync should not decide the fork's architecture under time pressure.

## Protect the project format

ChooChooTracker has its own project contract: eight fixed tracks, one AY instance per track, and per-track mixer levels. Compatibility with ChipNomad project files is not a goal.

Follow these rules:

- A project created by a released ChooChooTracker version must remain loadable.
- A missing new field receives a safe default.
- An incompatible ChooChooTracker format change requires a new format version.
- Saving and loading must preserve every ChooChooTracker field.
- ChipNomad files are references or explicit import fixtures, not implicit compatibility tests.

Continue reviewing upstream format changes for useful fixes, but do not create a ChipNomad migration path unless one is explicitly required.

## Keep Mutable Instruments frozen

The Braids, Plaits, Clouds, and stmlib files used by ChooChooTracker are snapshots, not live dependencies. Their source commits are documented in `dev_readme.md` and the ignored `inspirations/` directory.

Put adaptations in `BraidsVoice`, `PlaitsVoice`, or the ChooChooTracker mixer whenever possible. Changes needed for host compilation or the 96 kHz Clouds adaptation should remain small, commented, and tested. The original DSP should stay recognizable so comparisons with the source remain useful.

Change a snapshot only to:

- fix a DSP bug reproduced by a test
- fix compilation on a supported target
- remove unused code after checking licenses and linked symbols

Keep such a patch in its own commit, record the original file, and add a test. Mutable Instruments does not need to be checked during every ChipNomad sync.

## Minimum validation

Tests must pass on `main` before a sync. After importing changes, run at least these commands from `tracker`:

```sh
make test
make windows
```

A PortMaster packaging-only change does not require every DSP test, but the final ZIP must be rebuilt and checked:

```sh
make -f Makefile.portmaster PortMaster-deploy
unzip -t ../releases/choochootracker.zip
```

On RG353V, check controls, play, stop, save, and exit. Audio changes need an AY project, a Braids or Plaits project, and a hybrid project. Measure load on the console rather than relying only on desktop results.

Every imported upstream bug fix should leave behind a small test that fails without it. Trivial documentation and packaging changes do not need a new test.

## Publish a sync

After validation:

1. Update the sync log below.
2. Review the diff without relying on the upstream commit's stated intent.
3. Merge the branch into `main` without rewriting public history.
4. Delete the sync branch.
5. Keep the upstream SHA in adapted or cherry-picked commits.

Never rebase ChooChooTracker's public `main` onto ChipNomad. After several releases, the histories will have diverged enough that rebasing would make contributions and bug reports harder to follow.

## Sync log

Add a row for every review, including reviews that import nothing. This prevents the same range from being reviewed twice.

| Date | Last ChipNomad SHA reviewed | Commits imported | Decision |
| --- | --- | --- | --- |
| YYYY-MM-DD | `<sha>` | none or SHA list | Initial base, imported fixes, or skipped changes with a short reason |

The last reviewed SHA becomes the starting point for the next review. A row can simply say that the current C++ rewrite does not contain a fix ChooChooTracker needs.

## When to stop syncing

As the architectures diverge, commit-by-commit review will eventually cost more than it returns. At that point, reading ChipNomad release notes and bug reports is enough. Useful ideas can still be reimplemented without trying to bring the trees back together.

The fork is healthy when its projects load, its tests pass, and users can work from the announced ChipNomad base. The number of shared commits is not a useful health metric.
