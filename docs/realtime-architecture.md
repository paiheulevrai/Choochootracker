# Real-time audio architecture

Date: 2026-08-24

## Purpose

This note records the boundary between the tracker UI and the audio callback.

The immediate reason is live stick modulation and motion recording. The same
rules also apply to project editing, transport, saves, master effects and any
future automation feature.

## Guarantees now in place

### The audio thread does not edit the project

The project is read and edited by the UI, and can be serialized while the
audio callback is running. Writing phrase FX from the callback makes that
shared object mutable from two threads. Atomic flags around the write do not
make the project data safe.

This is especially visible with motion record: the callback can decide what
to record, but it must not insert or clear an FX cell itself. A save taken at
the wrong time could otherwise observe a partially changed project.

### The audio callback does not allocate

Mix and master-effect buffers can currently grow from rendering code. That may
only happen when a buffer size changes, but it is still the wrong place for a
resize. Allocation can take an unbounded amount of time and can cause an
audible dropout on the RG353V.

The callback must only read, write and process memory that already exists.

## Target model

Keep this deliberately small. There is no need for a generic event framework
or a lock around the whole engine.

### Ownership

- The UI owns `Project`. It is the only thread allowed to edit or save it.
- The audio engine owns an execution state. It contains the playback position,
  voices, live-controller values and the project data needed for rendering.
- The UI publishes project snapshots through three fixed slots. At a tick,
  audio adopts the newest complete snapshot, then applies coalesced controls,
  then transport commands. Editing while playing therefore takes effect on
  the next tick without sharing the UI project directly.

The useful rule is simple: the callback never reaches back into UI-owned data.

### UI to audio: command queue

A fixed-size single-producer/single-consumer queue carries small commands from
the UI to the callback. Examples are transport changes, live stick values,
parameter changes that are safe during playback, and a request to refresh the
execution state at a defined boundary.

Each command is a plain fixed-size value. Pushing and consuming one must not
allocate or lock. The UI is the producer; the callback is the consumer.

`Stop` is an atomic priority request. Starts, phrase queues and preview
commands preserve FIFO order. Loop, mute/solo and project refresh are
coalesced: the latest value wins. The callback publishes a separate playback
status snapshot for UI cursors and meters; UI code must not read its mutable
`PlaybackState` directly.

### Audio to UI: motion queue

Motion recording uses the reverse direction. At the sequencer tick, audio
code produces a small event containing:

- phrase location and track,
- destination FX,
- absolute FX value,
- requested operation: record or erase.

The UI consumes those events and applies the existing collision policy to the
three FX columns. It is therefore also the thread that marks the project dirty
and redraws the phrase screen.

The queue must be bounded. If it fills, the engine should set a visible
overflow indicator and drop new events rather than blocking audio. Queue size
is chosen from the maximum tick rate and the longest expected UI stall, then
checked on hardware.

### Audio buffers

Audio and master-effect buffers are allocated before the device starts, or
during an explicit reconfiguration while rendering is stopped. Their capacity
is based on the maximum callback size supported by the selected backend.

If a backend delivers a larger buffer unexpectedly, the callback must not
resize. It should process a bounded safe amount or produce silence for the
excess, set an atomic diagnostic flag, and let the UI request a safe
reconfiguration. That is preferable to an allocation in the real-time path.

## Implementation status

The project handoff, fixed command queues, motion queue, playback status
snapshot and render-buffer diagnostics are implemented. Engine tests cover
handoff ordering, command behavior and the instrument catalogue. The remaining
validation is operational: desktop stress runs and an RG353V session with a
dense project, effects, live editing, motion recording and saving.

## Related observations

Audio-rate modulation deserves measurement before redesign. Its expensive path
is conditional and may be fine on the target hardware. It should be profiled
with representative songs before work is scheduled around it.

Voice-rendering code has visible duplication. That is a maintenance concern,
but it should not be folded into the real-time safety work. Extract common
pieces only when a repeated bug or a measured hot path gives a concrete target.

Trigger mapping should eventually use logical actions such as `motionRecord`
and `motionErase`, with SDL, Web and gptokeyb adapters feeding the same action.
That is a portability and UX issue, separate from callback safety.

## Validation

Unit tests can check queue ordering, overflow behavior, motion event contents
and buffer-boundary handling. They cannot prove real-time behavior.

Use ThreadSanitizer on a desktop build to find unintended shared accesses, and
use AddressSanitizer for memory errors. Neither replaces a hardware run. The
final check is a PortMaster build played on the RG353V while editing, recording
motion and saving projects.
