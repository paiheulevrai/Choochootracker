"""Prepare SID programs and bounded register traces; does not modify app banks.

Run: python3 tools/sid_prep/prepare.py OUTPUT_DIRECTORY
Project-authored code and original recipes: MIT. Imported sources keep notices.
"""
from pathlib import Path
import ast
import hashlib
import json
import math
import sys

ROOT = Path(__file__).resolve().parent
WAVES = {"TRIANGLE": 0x10, "SAWTOOTH": 0x20, "PULSE": 0x40, "NOISE": 0x80}


def sidkit():
    """Read literal preset definitions without executing downloaded Python."""
    tree = ast.parse((ROOT / "sources/sidkit/sid_sfx/presets.py").read_text())
    node = next(n.value for n in tree.body if isinstance(n, ast.Assign)
                and any(isinstance(t, ast.Name) and t.id == "PRESETS" for t in n.targets))
    result = []
    for key, call in zip(node.keys, node.values):
        assert isinstance(call, ast.Call) and isinstance(call.func, ast.Name)
        assert call.func.id == "SfxPatch" and not call.args
        patch = {}
        for item in call.keywords:
            if isinstance(item.value, ast.Attribute):
                assert item.arg == "waveform" and isinstance(item.value.value, ast.Name)
                assert item.value.value.id == "Waveform"
                patch[item.arg] = item.value.attr
            else:
                patch[item.arg] = ast.literal_eval(item.value)
        assert patch["name"] == ast.literal_eval(key)
        result.append(patch)
    return result


def goattracker(path):
    data = path.read_bytes()
    if data[:4] != b"GTI5" or len(data) < 29:
        raise ValueError(f"Unsupported/truncated GoatTracker file: {path}")
    pos = 29
    tables = {}
    for name in ["wave", "pulse", "filter", "speed"]:
        if pos >= len(data):
            raise ValueError(f"Missing {name} table: {path}")
        length = data[pos]
        pos += 1
        if pos + 2 * length > len(data):
            raise ValueError(f"Truncated {name} table: {path}")
        tables[name] = list(zip(data[pos:pos + length], data[pos + length:pos + 2 * length]))
        pos += 2 * length
    if pos != len(data):
        raise ValueError(f"Trailing data: {path}")
    return dict(path=str(path.relative_to(ROOT)), license="CC-BY-4.0",
                status="parsed candidate; table playback and source attribution review pending",
                header=list(data[4:13]), tables=tables,
                payload_sha256=hashlib.sha256(data[4:13] + data[29:]).hexdigest())


def originals():
    # Eight musically different families, four distinct programs in each.
    # These are original recipes, not extractions or game-authentic claims.
    families = [
        ("Round Bass", "TRIANGLE", (0, 8, 6, 4), "lowpass", 45, 4),
        ("Moving Pulse", "PULSE", (1, 6, 10, 5), "lowpass", 100, 8),
        ("Saw Pluck", "SAWTOOTH", (0, 7, 0, 5), "lowpass", 150, 11),
        ("Soft Pad", "PULSE", (9, 7, 12, 8), "lowpass", 85, 3),
        ("Arp Bell", "TRIANGLE", (0, 10, 0, 7), "off", 100, 0),
        ("Noise Drum", "NOISE", (0, 4, 0, 3), "bandpass", 80, 7),
        ("Bright Lead", "SAWTOOTH", (1, 5, 11, 4), "off", 150, 0),
        ("Pulse Chord", "PULSE", (0, 7, 8, 5), "lowpass", 180, 6),
    ]
    result = []
    for name, wave, adsr, mode, cutoff, resonance in families:
        for variant in range(4):
            patch = dict(name=f"{name} {variant + 1}", waveform=wave, voice=1,
                         attack=adsr[0], decay=min(15, adsr[1] + variant), sustain=adsr[2], release=adsr[3],
                         pw_hi=[2, 4, 8, 12][variant], duration_frames=75,
                         filter_mode=mode, filter_cutoff=max(0, cutoff - variant * 12),
                         filter_resonance=resonance, vibrato_rate=4.0 + variant,
                         vibrato_depth=0 if wave == "NOISE" else variant * 4,
                         note_relative=True, original=True)
            if wave == "PULSE":
                patch["pulse_sweep"] = dict(depth=128 + variant * 128, rate=0.3 + variant * 0.2)
            if name in ("Arp Bell", "Pulse Chord"):
                patch["arpeggio"] = [[0, 4, 7], [0, 3, 7], [0, 7, 12], [0, 5, 9]][variant]
            if name in ("Round Bass", "Saw Pluck"):
                patch["filter_cutoff_sweep"] = max(1, cutoff // 4 + variant * 3)
            if wave == "NOISE":
                patch.update(freq_hi=8 + variant * 16, freq_lo=0, note_relative=False,
                             duration_frames=8 + variant * 4)
            result.append(patch)
    return result


def trace(patch, midi=60):
    # SIDkit cutoff is the HIGH register, pulse width follows its native register
    # masking. Preserve those semantics rather than silently treating them as Hz.
    wave = WAVES[patch["waveform"]]
    voice = patch.get("voice", 1) - 1
    assert 0 <= voice < 3
    base = voice * 7
    adsr = [patch.get(k, default) for k, default in zip(
        ["attack", "decay", "sustain", "release"], [0, 4, 0, 0])]
    assert all(isinstance(v, int) and 0 <= v <= 15 for v in adsr)
    freq = (patch.get("freq_hi", 0x10) << 8) | patch.get("freq_lo", 0)
    if patch.get("note_relative"):
        freq = round(440 * 2 ** ((midi - 69) / 12) * 2**24 / 985248)
    target = (patch.get("sweep_target_hi", 0) << 8) | patch.get("sweep_target_lo", 0)
    duration = 125 if patch.get("loop") else patch.get("duration_frames", 10)
    duration = min(125, max(1, duration))
    events = []
    def reg(frame, address, value):
        assert 0 <= address <= 24 and 0 <= value <= 255
        events.append((frame, address, value))
    reg(0, base + 4, 0x08)  # Explicit test-bit reset before gate.
    reg(0, base + 5, adsr[0] * 16 + adsr[1])
    reg(0, base + 6, adsr[2] * 16 + adsr[3])
    mode = {"off": 0, "lowpass": 1, "bandpass": 2, "highpass": 4}[patch.get("filter_mode", "off")]
    reg(0, 0x17, patch.get("filter_resonance", 15) * 16 + ((1 << voice) if mode else 0))
    reg(0, 0x18, mode * 16 + 15)
    for frame in range(duration):
        elapsed = frame / 50
        progress = min(1, frame / max(1, patch.get("sweep_frames", 0) or duration))
        pitch = freq
        if target:
            pitch = freq * (target / max(1, freq)) ** progress if patch.get("sweep_type", "exponential") == "exponential" else freq + (target - freq) * progress
        if "arpeggio" in patch:
            arp = patch["arpeggio"]
            pitch *= 2 ** (arp[frame % len(arp)] / 12)
        pitch += patch.get("vibrato_depth", 0) * math.sin(2 * math.pi * patch.get("vibrato_rate", 0) * elapsed)
        pitch = max(0, min(65535, round(pitch)))
        reg(frame, base, pitch & 255); reg(frame, base + 1, pitch >> 8)
        pulse = (patch.get("pw_hi", 4) & 15) << 8
        if "pulse_sweep" in patch:
            sweep = patch["pulse_sweep"]
            pulse += round(sweep["depth"] * math.sin(2 * math.pi * sweep["rate"] * elapsed))
        pulse = max(1, min(4094, pulse)) if patch.get("original") else pulse
        reg(frame, base + 2, pulse & 255); reg(frame, base + 3, pulse >> 8)
        cutoff = patch.get("filter_cutoff", 0x90)
        if patch.get("filter_cutoff_sweep"):
            cutoff += (patch["filter_cutoff_sweep"] - cutoff) * frame / max(1, duration - 1)
        reg(frame, 0x15, 0); reg(frame, 0x16, round(cutoff))
        if frame == 0:
            reg(frame, base + 4, wave | 1)
    reg(duration, base + 4, wave)
    return "".join(f"{t} {a} {v}\n" for t, a, v in events)


def main():
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / "source-manifest.json").read_text())
    for entry in manifest["files"]:
        assert hashlib.sha256((ROOT / entry["path"]).read_bytes()).hexdigest() == entry["sha256"], entry["path"]
    assert hashlib.sha256((ROOT / manifest["core"]["path"]).read_bytes()).hexdigest() == manifest["core"]["sha256"]
    builtins = sidkit(); authored = originals()
    gti = [goattracker(p) for p in sorted((ROOT / "sources/instruments").rglob("*.ins"))]
    for bank, patches in [("originals", authored), ("sidkit", builtins)]:
        (out / f"{bank}.json").write_text(json.dumps(patches, indent=2) + "\n")
        for index, patch in enumerate(patches):
            for note in ([48, 60, 72] if patch.get("note_relative") else [60]):
                (out / f"{bank}-{index:02d}-{note}.regs").write_text(trace(patch, note))
    report = dict(original_programs=len(authored), mit_sidkit_builtins=len(builtins),
                  cc_by_gti_candidates=len(gti), cc_by_unique_payloads=len({p["payload_sha256"] for p in gti}),
                  cc_by_families=["grand-piano", "acoustic-guitar", "violin"],
                  gti_candidates=gti, app_integration=False, hardware_variants_emulated=["generic 6581"],
                  limitations="GTI tables parsed but not rendered. SIDkit traces are an adaptation, not a verified equivalent player. No chip revision filters implemented.")
    (out / "inventory.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "gti_candidates"}, indent=2))


if __name__ == "__main__":
    main()
