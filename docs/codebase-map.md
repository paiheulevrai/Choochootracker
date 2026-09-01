# Codebase map

This is an intentionally small map of the code that changes behaviour. It is
not a file-by-file dependency graph: generated full graphs obscure the paths
where a change can affect audio, project data, or a platform build.

```mermaid
flowchart LR
  subgraph App[Tracker application]
    Main[platforms/shared/main.cpp]
    AppCode[src/app.cpp]
    Screens[src/screens/]
    Files[src/corelib/ + src/import/]
    Audio[src/audio_manager.cpp]
  end

  subgraph Engine[chipnomad_lib]
    State[ChipNomadState]
    Project[project*\nformat + instruments]
    Playback[playback*\nticks + tracker FX]
    Voices[synth/\nvoices + effects]
    Chips[chips/\nAY emulation]
    Export[export/\noffline WAV]
  end

  subgraph Platform[Platform adapters]
    SDL[platforms/sdl2/]
    Web[platforms/web/]
    Other[Android / SDL 1.2 / PortMaster]
  end

  Main --> AppCode
  Main --> SDL
  AppCode <--> Screens
  Screens <--> Files
  AppCode <--> Audio
  AppCode <--> State
  Audio --> State
  State --> Project
  State --> Playback
  Playback --> Voices
  Playback --> Chips
  Export --> State
  SDL --> Audio
  Web --> Audio
  Other --> Audio
```

## Real-time boundary

This is the most important graph when reviewing a change. UI code owns the
editable `project`; the audio callback reads `audioProject`. Do not add direct
UI writes from audio code, allocation, filesystem access, or locks on the
audio side.

```mermaid
sequenceDiagram
  participant UI as Screens / app.cpp
  participant State as ChipNomadState
  participant Queue as AudioCommandQueue
  participant Audio as Audio callback
  participant Voice as Playback + voices

  UI->>State: edit project
  UI->>Queue: project refresh / transport command
  Audio->>Queue: consume at tick boundary
  Audio->>State: update audioProject snapshot
  Audio->>Voice: playback tick then render
  Voice-->>Audio: stereo float samples
  Audio-->>UI: PlaybackStatus + motion events
```

## Ownership map

Use this before changing imports, loading, samples, or export. Solid arrows
mean ownership; dashed arrows mean a temporary or read-only view.

```mermaid
flowchart TD
  AppState[ChipNomadState] -->|owns| Project[Project]
  Project -->|owns| Instruments[Instrument array]
  Instruments -->|owns when present| SampleData[PCM / AY sample buffers]
  AppState -->|owns| Mix[Mix + reverb + delay buffers]
  AppState -->|owns| VoiceObjects[Voices, chips, effects, command queue]
  AppState -. snapshot .-> AudioProject[audioProject]
  Exporter -->|owns| ExportState[offline ChipNomadState]
  Exporter -. borrows sample buffers .-> Project
```

## Review paths

Start at the narrowest path that contains the change.

| Change | Read first | Then trace |
| --- | --- | --- |
| Tracker UI or navigation | `tracker/src/screens/`, `screens.cpp` | `app.cpp`, project/file callbacks |
| Project format or sample ownership | `chipnomad_lib/project*.cpp` | `screen_project.cpp`, importers, `chipnomadDestroy()` |
| Playback or FX | `playback.cpp`, `playback_fx_*.cpp` | `chipnomadRender()`, targeted voice |
| New synth parameter | matching `synth/*_voice.cpp` | `project_instruments.*`, instrument screen, FX metadata |
| Audio glitches or races | `audio_manager.cpp`, `chipnomad_lib.cpp` | queues, buffer allocation, platform callback |
| Platform-specific breakage | matching `tracker/platforms/*/` | `Makefile.*`, packaging and assets |

## Practical use

For a suspected improvement, mark its entry point on the first graph, then
follow every outgoing arrow until ownership or thread boundary changes. If a
proposed change crosses into the audio callback, require a targeted test and a
desktop stress run. Keep a generated Doxygen/Graphviz graph as a local search
aid only; this hand-maintained map should remain the readable architectural
reference.
