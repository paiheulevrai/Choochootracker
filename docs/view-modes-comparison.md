# Simplified on-the-fly workflow and historical comparison

## Current behaviour

There is now one Simplified choice, alongside Classic and Groovebox. It keeps
Reip's pattern editor workflow but writes directly to chains. Opening a bar
does not allocate. The first write isolates only a shared phrase reference,
without a popup; every Song occurrence of that chain remains linked.
Length edits also affect all occurrences. Doubling copies references and
defers phrase isolation until editing.

At the end of an editing gesture, the edited bar may reuse a referenced
phrase with exactly identical stored content. The bar transpose is retained.
Only a known unused editor-created copy is cleared; retained chain/Pattern references
are checked. Changed phrase identities may change MOD visit-counter behaviour.
No allocation, content search or reconciliation runs in the audio callback.

New projects have Classic backing regardless of the default editor. Historical
Pattern-format files are reconciled once on load, before replacing the live
project. Conversion failure does not replace the current project or rewrite
the source file. Historical conversion helpers and serialized arrays/maps
remain for import compatibility; they are no longer another selectable workflow.

Validation of this revision: Windows application and native harness build;
121 focused doctest cases / 101,186 assertions pass. All three native selectors
below pass, with the canonical selector repeated three times. Actual inputs
cover automatic isolation, successive edits, identical-content reuse, desktop
key jazz, full-slot refusal, canonical new projects, successful legacy import
and failed import preserving the live project and source file. The harness
keeps SDL callbacks paused during project-load tests because it renders audio
synchronously.

The historical comparison below describes the earlier four-choice prototype,
not the current interface.

This is a proposal for discussion with Reip, not a replacement of his work or
a claim that his conversion is broken. His pattern workflow is the UX reference.
The prototype integrates PR #179 through `d5d68825`, retaining its Simplified
mode, conversion helpers, tests and classic merge-up.

## What is being compared?

| | Reip Simplified | Simple C | Groovebox |
|---|---|---|---|
| Editor | Song / multi-bar Pattern | Song / chain presented as Pattern | SCENES / eight-track PATTERN grid |
| Playback backing | Patterns in Simplified mode | Classic Song / Chains / Phrases | Classic Song / Chains / Phrases |
| Switching | Explicit bidirectional conversion/reconciliation | Session editor flag, no conversion | Session editor flag, no conversion |
| Shared bars | Automatic isolation on editing setup | Explicit independence popup or Song deep clone before local edits | Explicit independence confirmation before local edits |
| Transpose | Conversion can bake signed transpose into notes | Original per-reference transpose retained | Original per-reference transpose retained |
| Browsing existing canonical material | May allocate isolated phrases | Does not allocate | Does not allocate |

Simple C means **Simplified Canonical**, a working name. One chain is shown as
a pattern; each chain row is a bar. It uses the real Phrase editor, not a
parallel note editor. Independently owned data is directly editable. Shared
edits open a Make pattern independent / Cancel popup on button release.
Confirmation explicitly clones the chain and all its referenced bars, then the
musician repeats the edit. Cancel preserves the project. No permanent header
control is added. Song deep cloning and Classic shared editing remain available.

To isolate an occurrence in Song: SHIFT + OPT selects; hold SHIFT and press
EDIT twice without releasing SHIFT. This deep-clones the chain and all its
referenced phrases, including retained references after gaps.

Simple C selection merge-up creates fresh chains, preserves source chains and
phrase/transpose references, and refuses overlength, retained gaps or exhaustion
before changing any selected track. Reip's original classic/simplified merge
remains available in its original modes. These are deliberately different
structural operations, not interchangeable sound-neutral algorithms.

Groovebox uses the same canonical backing. SCENES retains Song selection,
copy/paste, cloning, row editing and its double-OPT Live toggle. PATTERN provides
note/instrument/volume/three-FX editing, explicit independence, length tools,
per-track live launch, standard transport, and the existing waveform sidebar.
Short tracks show read-only repeated bars; browsing follows the longest track
without materializing duplicates. This is a structural preview, not a common
musical clock under HOP, groove or differing speeds.

## Why explore this alternative?

The maintainer prefers that changing view not silently reshape the arrangement.
Retaining sharing lets musicians edit a common motif globally in Classic and
explicitly clone only their local variations. Reip's automatic isolation is more
immediate for local edits. Neither workflow is inherently more musical.

On a new song with independent bars, workflows are very similar. The difference
appears when references are reused. Both currently have 255 patterns/chains,
16 bars per pattern and 1,024 shared phrase slots across all tracks. Automatic
isolation can consume more of that pool.

**This prototype does not remove duplicate serialized structures.** It still
uses the combined writer with historical pattern arrays/maps. Removing those,
importing existing Simplified saves and validating older-release readers are
separate migration work. ID-dependent conditional FX can change after cloning
or merging; no unconditional audio-equivalence claim is made.

## Observed results

The initial experiment used three fixtures included under
`tracker/packaging/common/projects/arrangement-tests`.

| Fixture | Song cells | Bars across occurrences | Transposed bars | Shared bars | Extra phrases: Simple C / Reip |
|---|---:|---:|---:|---:|---:|
| Ambient_01 | 95 | 1,436 | 32 | 1,435 | 0 / 4 |
| ChipTek | 5 | 32 | 4 | 27 | 0 / 4 |
| ChooChooseAcid | 58 | 928 | 125 | 928 | 0 / 33 |

Browsing and direct canonical-editor switching left the complete Project
unchanged. Reip's no-edit forward/reverse conversion preserved Song and Chains
on these fixtures too. Extra phrases are not evidence of damage.

Earlier fresh-process 3-second, 48 kHz stereo renders were sample-exact for
Classic / Simple C / Reip. **That is only a smoke test.** Full-song verification
is unfinished: Ambient Classic and Simple C reached their final arranged rows
and stopped at 312.453 seconds; the complete three-mode/all-fixture comparison
was not completed. No whole-song equivalence conclusion should be drawn.

## Reproduce

From `tracker`, build the Windows app/assets and native integration harness:

```sh
make -j4 -f Makefile.windows windows native-chip-screenshots
make -j4 -f Makefile.test
```

Use MSYS2 UCRT64 on Windows. From `tracker/build/windows`, run:

```sh
mkdir -p comparison-captures
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
./native-chip-screenshots.exe comparison-captures --canonical-simple-only
./native-chip-screenshots.exe comparison-captures --groovebox-feedback-only
./native-chip-screenshots.exe comparison-captures --groovebox-only
```

These exercise actual app button-down/up events, allocation/refusal, shared
source preservation, transpose, length, BAR/PAT transport, Phrase clipboard,
save/reload, SCENES clipboard/live transport, read-only repetitions, playhead
colours and waveform alignment. Captures are developer output, not release
assets. An optional local console reproduction project may be supplied through
`CCT_GROOVEBOX_REPRO_PROJECT`; no private console project is distributed.

For manual testing, choose Project -> View -> Mode. The choices are Classic,
Groovebox and Simplified. From Classic, EDIT + LEFT selects Simplified;
another LEFT selects Groovebox. Keep copies of your projects.

The native harness also supports full-song rendering:

```sh
CCT_COMPARE_FIXTURE=Ambient_01 CCT_COMPARE_MODE=classic \
  ./native-chip-screenshots.exe comparison-captures --canonical-song-audio-only
```

See the argument contract in `tracker/tests/native_chip_ui.cpp` before running;
full-song raw audio can consume hundreds of megabytes. Natural stop, final
arranged-row coverage and effect tails matter, not just a short opening render.

Hardware input/audio/suspend, console CPU cost, complete-song equivalence and
historical file compatibility still need independent validation.

Previous four-choice integration validation: Windows comparison executable builds; all three
native comparison selectors above pass, including shared-popup cancel/confirm.
The focused doctest selection passes 115 cases / 101,100 assertions:

```sh
./build/tests/run_tests.exe \
  --test-case="*pattern*,*Pattern*,*Groovebox*,*groovebox*,*View*,*view*,*Live*,*live*"
```

The complete Windows suite was also attempted, but is not green: unrelated
import/soundbank/support-report/USER-library tests encounter Unix shell commands,
Windows path/line-ending expectations and preset-library failures. The affected
test files are unchanged by this contribution. The make recipe also uses a
POSIX conditional through the Windows shell. Do not report full-suite success;
Linux CI and baseline comparison are still required.
