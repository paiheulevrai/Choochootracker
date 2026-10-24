"""SFX patch schema — synth-oriented description of a one-shot SID sound effect.

Maps to the 7-byte format used by the C64 game engine:
  voice(1B), CR(1B), freq_hi(1B), freq_lo(1B), AD(1B), SR(1B), pw_hi(1B)
"""

from __future__ import annotations

import enum
import json
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional


class Waveform(enum.Enum):
    TRIANGLE = 0x10
    SAWTOOTH = 0x20
    PULSE = 0x40
    NOISE = 0x80


# SID ADSR timing tables (milliseconds, from datasheet)
ATTACK_MS = [2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000]
DECAY_RELEASE_MS = [6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000]


@dataclass
class SfxPatch:
    """One-shot SID sound effect patch.

    Synth-oriented parameters that map directly to SID registers.
    """

    name: str
    voice: int = 1  # 1, 2, or 3

    # Waveform
    waveform: Waveform = Waveform.PULSE

    # Frequency (16-bit SID frequency register)
    freq_hi: int = 0x10
    freq_lo: int = 0x00

    # Envelope (nibble values 0-15)
    attack: int = 0
    decay: int = 4
    sustain: int = 0
    release: int = 0

    # Pulse width high byte (only relevant for pulse waveform)
    pw_hi: int = 0x04

    # Duration in frames for suppression timer (informational for export)
    duration_frames: int = 10

    # Pitch sweep (software-only — not part of the 7-byte hardware export).
    # If sweep_target is 0 or None, no sweep is applied (backward compatible).
    sweep_target_hi: int = 0
    sweep_target_lo: int = 0
    sweep_frames: int = 0  # Number of frames for the sweep (0 = use duration_frames)
    sweep_type: str = "exponential"  # "linear" or "exponential"

    # Vibrato (periodic frequency modulation — software + engine support).
    # Applied as freq += depth * sin(2π * rate * t) each frame.
    vibrato_rate: float = 0.0  # Hz (0 = no vibrato)
    vibrato_depth: int = 0  # SID frequency units (peak deviation)

    # Filter (preview-only — engine handles filter via separate registers)
    filter_mode: str = "off"  # "off", "lowpass", "bandpass", "highpass"
    filter_cutoff: int = 0x90  # SID SIDFCHI value (0-255)
    filter_resonance: int = 0xF  # SID resonance nibble (0-15)
    filter_cutoff_sweep: int = 0  # Sweep target for cutoff (0 = no sweep)

    # Loop: when True, gate stays open indefinitely (sustained/held sounds).
    # Preview renders loop_preview_seconds of audio instead of one-shot envelope.
    loop: bool = False
    loop_preview_seconds: float = 5.0

    # Optional description for documentation
    description: str = ""

    def __post_init__(self):
        if self.voice not in (1, 2, 3):
            raise ValueError(f"voice must be 1, 2, or 3, got {self.voice}")
        if isinstance(self.waveform, str):
            self.waveform = Waveform[self.waveform.upper()]
        for name, val, hi in [
            ("freq_hi", self.freq_hi, 0xFF),
            ("freq_lo", self.freq_lo, 0xFF),
            ("pw_hi", self.pw_hi, 0xFF),
            ("sweep_target_hi", self.sweep_target_hi, 0xFF),
            ("sweep_target_lo", self.sweep_target_lo, 0xFF),
        ]:
            if not 0 <= val <= hi:
                raise ValueError(f"{name} must be 0-{hi}, got {val}")
        for name, val in [
            ("attack", self.attack),
            ("decay", self.decay),
            ("sustain", self.sustain),
            ("release", self.release),
        ]:
            if not 0 <= val <= 15:
                raise ValueError(f"{name} must be 0-15, got {val}")
        if self.sweep_type not in ("linear", "exponential"):
            raise ValueError(f"sweep_type must be 'linear' or 'exponential', got {self.sweep_type!r}")
        if self.vibrato_rate < 0:
            raise ValueError(f"vibrato_rate must be >= 0, got {self.vibrato_rate}")
        if not 0 <= self.vibrato_depth <= 0xFFFF:
            raise ValueError(f"vibrato_depth must be 0-65535, got {self.vibrato_depth}")

    @property
    def cr_byte(self) -> int:
        """Control register: waveform | gate."""
        return self.waveform.value | 0x01

    @property
    def ad_byte(self) -> int:
        """Attack/Decay register."""
        return (self.attack << 4) | self.decay

    @property
    def sr_byte(self) -> int:
        """Sustain/Release register."""
        return (self.sustain << 4) | self.release

    @property
    def frequency(self) -> int:
        """16-bit SID frequency value."""
        return (self.freq_hi << 8) | self.freq_lo

    @frequency.setter
    def frequency(self, val: int):
        self.freq_hi = (val >> 8) & 0xFF
        self.freq_lo = val & 0xFF

    @property
    def sweep_target(self) -> int:
        """16-bit SID frequency sweep target (0 = no sweep)."""
        return (self.sweep_target_hi << 8) | self.sweep_target_lo

    @sweep_target.setter
    def sweep_target(self, val: int):
        self.sweep_target_hi = (val >> 8) & 0xFF
        self.sweep_target_lo = val & 0xFF

    @property
    def has_sweep(self) -> bool:
        return self.sweep_target > 0

    @property
    def has_vibrato(self) -> bool:
        return self.vibrato_rate > 0 and self.vibrato_depth > 0


    def to_bytes(self) -> bytes:
        """Encode as 7-byte SFX data (matches game engine format).

        Note: sweep params are preview-only (software concept) and not
        included in the hardware export. The C64 engine handles sweeps
        via its own per-frame frequency update loop.
        """
        return bytes([
            self.voice,
            self.cr_byte,
            self.freq_hi,
            self.freq_lo,
            self.ad_byte,
            self.sr_byte,
            self.pw_hi,
        ])

    def to_dict(self) -> dict:
        d = asdict(self)
        d["waveform"] = self.waveform.name
        return d

    @classmethod
    def from_dict(cls, d: dict) -> SfxPatch:
        d = dict(d)
        if "waveform" in d and isinstance(d["waveform"], str):
            d["waveform"] = Waveform[d["waveform"].upper()]
        return cls(**d)

    def save_json(self, path: str | Path):
        Path(path).write_text(json.dumps(self.to_dict(), indent=2) + "\n")

    @classmethod
    def load_json(cls, path: str | Path) -> SfxPatch:
        return cls.from_dict(json.loads(Path(path).read_text()))

    @classmethod
    def from_bytes(cls, data: bytes, name: str = "unnamed") -> SfxPatch:
        """Parse 7-byte SFX data back into a patch."""
        if len(data) != 7:
            raise ValueError(f"Expected 7 bytes, got {len(data)}")
        voice, cr, fh, fl, ad, sr, pw = data
        waveform = Waveform(cr & 0xF0)
        return cls(
            name=name,
            voice=voice,
            waveform=waveform,
            freq_hi=fh,
            freq_lo=fl,
            attack=(ad >> 4) & 0xF,
            decay=ad & 0xF,
            sustain=(sr >> 4) & 0xF,
            release=sr & 0xF,
            pw_hi=pw,
        )


# Frequency helpers
def hz_to_sid_freq(hz: float, clock: float = 985248.0) -> int:
    """Convert Hz to 16-bit SID frequency register value.

    Default clock is PAL (985248 Hz). NTSC is 1022727 Hz.
    """
    return min(0xFFFF, max(0, round(hz * (1 << 24) / clock)))


def sid_freq_to_hz(sid_freq: int, clock: float = 985248.0) -> float:
    """Convert SID frequency register to Hz."""
    return sid_freq * clock / (1 << 24)


# MIDI note to Hz
def note_to_hz(note: int) -> float:
    """MIDI note number to Hz (A4=69=440Hz)."""
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def note_to_sid_freq(note: int, clock: float = 985248.0) -> int:
    """MIDI note number to SID frequency register."""
    return hz_to_sid_freq(note_to_hz(note), clock)
