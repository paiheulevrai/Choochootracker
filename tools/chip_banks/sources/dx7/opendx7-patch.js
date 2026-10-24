// OpenDX7 — FM Synthesizer (MIT License)
// Copyright (c) 2026 Keith Adler
// DX7 Patch data structures and SysEx parsing
// Handles both packed (32-voice bulk dump) and unpacked (single voice) formats

export function createDefaultOperator() {
  return {
    egRate1: 99, egRate2: 99, egRate3: 99, egRate4: 99,
    egLevel1: 99, egLevel2: 99, egLevel3: 99, egLevel4: 0,
    kbdLevelScaleBP: 39, // C3
    kbdLevelScaleLD: 0,
    kbdLevelScaleRD: 0,
    kbdLevelScaleLC: 0, // -LIN
    kbdLevelScaleRC: 0, // -LIN
    kbdRateScaling: 0,
    ampModSens: 0,
    velSensitivity: 0,
    outputLevel: 0,
    oscMode: 0, // ratio
    freqCoarse: 1,
    freqFine: 0,
    detune: 7 // center
  };
}

export function createDefaultPatch() {
  const patch = {
    name: 'INIT VOICE',
    ops: [],
    pitchEgR1: 99, pitchEgR2: 99, pitchEgR3: 99, pitchEgR4: 99,
    pitchEgL1: 50, pitchEgL2: 50, pitchEgL3: 50, pitchEgL4: 50,
    algorithm: 0,
    feedback: 0,
    oscSync: false,
    lfoSpeed: 35, lfoDelay: 0,
    lfoPitchModDepth: 0, lfoAmpModDepth: 0,
    lfoSync: false, lfoWave: 0,
    pitchModSens: 0,
    transpose: 24
  };
  for (let i = 0; i < 6; i++) {
    const op = createDefaultOperator();
    // ops[] is in msfa/Dexed order: ops[5] is OP1, the sole carrier of algorithm 1.
    if (i === 5) {
      op.outputLevel = 99; // Only OP1 audible by default
      op.freqCoarse = 1;
    }
    patch.ops.push(op);
  }
  return patch;
}

// Parse a 32-voice SysEx bulk dump (4104 bytes total, 128 bytes per voice packed)
export function parseSyxBank(data) {
  const patches = [];

  // Validate SysEx header
  // F0 43 00 09 20 00 ... F7
  let offset = 0;
  if (data[0] === 0xF0) {
    // Skip SysEx header (6 bytes typically)
    if (data[1] === 0x43 && data[3] === 0x09) {
      offset = 6;
    } else {
      offset = 6; // Try anyway
    }
  }

  for (let v = 0; v < 32; v++) {
    const voiceOffset = offset + v * 128;
    if (voiceOffset + 128 > data.length) break;
    patches.push(parsePackedVoice(data, voiceOffset));
  }

  return patches;
}

// Parse a single packed voice (128 bytes, as in 32-voice bulk dump)
function parsePackedVoice(data, offset) {
  const patch = createDefaultPatch();
  const d = (i) => data[offset + i] || 0;

  // 6 operators, each 17 bytes packed. The DX7 SysEx stores OP6 first, and the
  // synthesis engine (and the built-in patches) use msfa/Dexed operator order,
  // where ops[0] is OP6 ... ops[5] is OP1 — matching the algorithm table indices.
  // So the packed data maps straight through: opIdx 0 (OP6) -> ops[0].
  for (let opIdx = 0; opIdx < 6; opIdx++) {
    const op = patch.ops[opIdx];
    const o = opIdx * 17;

    op.egRate1 = d(o + 0) & 0x7F;
    op.egRate2 = d(o + 1) & 0x7F;
    op.egRate3 = d(o + 2) & 0x7F;
    op.egRate4 = d(o + 3) & 0x7F;
    op.egLevel1 = d(o + 4) & 0x7F;
    op.egLevel2 = d(o + 5) & 0x7F;
    op.egLevel3 = d(o + 6) & 0x7F;
    op.egLevel4 = d(o + 7) & 0x7F;
    op.kbdLevelScaleBP = d(o + 8) & 0x7F;
    op.kbdLevelScaleLD = d(o + 9) & 0x7F;
    op.kbdLevelScaleRD = d(o + 10) & 0x7F;

    const byte11 = d(o + 11);
    op.kbdLevelScaleLC = byte11 & 0x03;
    op.kbdLevelScaleRC = (byte11 >> 2) & 0x03;

    const byte12 = d(o + 12);
    op.kbdRateScaling = byte12 & 0x07;
    op.detune = ((byte12 >> 3) & 0x0F);

    const byte13 = d(o + 13);
    op.ampModSens = byte13 & 0x03;
    op.velSensitivity = (byte13 >> 2) & 0x07;

    op.outputLevel = d(o + 14) & 0x7F;

    const byte15 = d(o + 15);
    op.oscMode = byte15 & 0x01;
    op.freqCoarse = (byte15 >> 1) & 0x1F;

    op.freqFine = d(o + 16) & 0x7F;

    // Clamp values
    op.egRate1 = Math.min(99, op.egRate1);
    op.egRate2 = Math.min(99, op.egRate2);
    op.egRate3 = Math.min(99, op.egRate3);
    op.egRate4 = Math.min(99, op.egRate4);
    op.egLevel1 = Math.min(99, op.egLevel1);
    op.egLevel2 = Math.min(99, op.egLevel2);
    op.egLevel3 = Math.min(99, op.egLevel3);
    op.egLevel4 = Math.min(99, op.egLevel4);
    op.kbdLevelScaleBP = Math.min(99, op.kbdLevelScaleBP);
    op.kbdLevelScaleLD = Math.min(99, op.kbdLevelScaleLD);
    op.kbdLevelScaleRD = Math.min(99, op.kbdLevelScaleRD);
    op.outputLevel = Math.min(99, op.outputLevel);
    op.freqFine = Math.min(99, op.freqFine);
    op.detune = Math.min(14, op.detune);
  }

  // Global parameters (bytes 102-127)
  const g = 102;
  patch.pitchEgR1 = d(g + 0) & 0x7F;
  patch.pitchEgR2 = d(g + 1) & 0x7F;
  patch.pitchEgR3 = d(g + 2) & 0x7F;
  patch.pitchEgR4 = d(g + 3) & 0x7F;
  patch.pitchEgL1 = d(g + 4) & 0x7F;
  patch.pitchEgL2 = d(g + 5) & 0x7F;
  patch.pitchEgL3 = d(g + 6) & 0x7F;
  patch.pitchEgL4 = d(g + 7) & 0x7F;

  const byte110 = d(g + 8);
  patch.algorithm = byte110 & 0x1F;

  const byte111 = d(g + 9);
  patch.feedback = byte111 & 0x07;
  patch.oscSync = ((byte111 >> 3) & 0x01) === 1;

  patch.lfoSpeed = d(g + 10) & 0x7F;
  patch.lfoDelay = d(g + 11) & 0x7F;
  patch.lfoPitchModDepth = d(g + 12) & 0x7F;
  patch.lfoAmpModDepth = d(g + 13) & 0x7F;

  const byte116 = d(g + 14);
  patch.lfoSync = (byte116 & 0x01) === 1;
  patch.lfoWave = (byte116 >> 1) & 0x07;
  patch.pitchModSens = (byte116 >> 4) & 0x07;

  patch.transpose = d(g + 15) & 0x7F;

  // Voice name (bytes 118-127, 10 ASCII chars)
  let name = '';
  for (let i = 0; i < 10; i++) {
    const ch = d(g + 16 + i);
    name += String.fromCharCode(ch >= 32 && ch < 127 ? ch : 32);
  }
  patch.name = name.trim();

  // Clamp global values
  patch.pitchEgR1 = Math.min(99, patch.pitchEgR1);
  patch.pitchEgR2 = Math.min(99, patch.pitchEgR2);
  patch.pitchEgR3 = Math.min(99, patch.pitchEgR3);
  patch.pitchEgR4 = Math.min(99, patch.pitchEgR4);
  patch.pitchEgL1 = Math.min(99, patch.pitchEgL1);
  patch.pitchEgL2 = Math.min(99, patch.pitchEgL2);
  patch.pitchEgL3 = Math.min(99, patch.pitchEgL3);
  patch.pitchEgL4 = Math.min(99, patch.pitchEgL4);
  patch.algorithm = Math.min(31, patch.algorithm);
  patch.feedback = Math.min(7, patch.feedback);
  patch.lfoSpeed = Math.min(99, patch.lfoSpeed);
  patch.lfoDelay = Math.min(99, patch.lfoDelay);
  patch.lfoPitchModDepth = Math.min(99, patch.lfoPitchModDepth);
  patch.lfoAmpModDepth = Math.min(99, patch.lfoAmpModDepth);
  patch.lfoWave = Math.min(5, patch.lfoWave);
  patch.pitchModSens = Math.min(7, patch.pitchModSens);
  patch.transpose = Math.min(48, patch.transpose);

  return patch;
}


// ============================================================
// Helper: configure an operator concisely
// ============================================================
function setOp(p, idx, cfg) {
  const op = p.ops[idx];
  if (cfg.r) { op.egRate1 = cfg.r[0]; op.egRate2 = cfg.r[1]; op.egRate3 = cfg.r[2]; op.egRate4 = cfg.r[3]; }
  if (cfg.l) { op.egLevel1 = cfg.l[0]; op.egLevel2 = cfg.l[1]; op.egLevel3 = cfg.l[2]; op.egLevel4 = cfg.l[3]; }
  if (cfg.out !== undefined) op.outputLevel = cfg.out;
  if (cfg.coarse !== undefined) op.freqCoarse = cfg.coarse;
  if (cfg.fine !== undefined) op.freqFine = cfg.fine;
  if (cfg.detune !== undefined) op.detune = cfg.detune;
  if (cfg.vel !== undefined) op.velSensitivity = cfg.vel;
  if (cfg.ams !== undefined) op.ampModSens = cfg.ams;
  if (cfg.mode !== undefined) op.oscMode = cfg.mode;
  if (cfg.krs !== undefined) op.kbdRateScaling = cfg.krs;
  if (cfg.bp !== undefined) op.kbdLevelScaleBP = cfg.bp;
  if (cfg.ld !== undefined) op.kbdLevelScaleLD = cfg.ld;
  if (cfg.rd !== undefined) op.kbdLevelScaleRD = cfg.rd;
  if (cfg.lc !== undefined) op.kbdLevelScaleLC = cfg.lc;
  if (cfg.rc !== undefined) op.kbdLevelScaleRC = cfg.rc;
}

function mkPatch(name, algo, fb, ops, globals) {
  const p = createDefaultPatch();
  p.name = name;
  p.algorithm = algo;
  p.feedback = fb;
  // createDefaultPatch() leaves OP1 (ops[5]) at output level 99 so INIT VOICE
  // makes sound. Patches must opt in to every operator they use, otherwise a
  // full-level INIT sine rides along with the sound.
  p.ops[5].outputLevel = 0;
  for (let i = 0; i < 6; i++) if (ops[i]) setOp(p, i, ops[i]);
  if (globals) Object.assign(p, globals);
  return p;
}

// ============================================================
// Built-in Bank — 32 patches with generic names
// All parameter values are original clean-room designs (not copies of DX7 ROM patches).
// ============================================================
export function generateFactoryPatches() {
  return [
    // Algorithm 5 is three carrier/modulator pairs: OP2>OP1, OP4>OP3, OP6>OP5,
    // which in this array is 4>5, 2>3, 0>1. An FM electric piano wants each
    // pair doing a different job rather than all three doing the same one.
    //
    //   pair A (0>1)  the tine. A modulator at fourteen times the carrier is
    //                 what makes the metallic ping, and it has to die inside
    //                 about a tenth of a second or the sound becomes a bell.
    //   pair B (2>3)  the body, at ratio 1, decaying slowly: the wooden part.
    //   pair C (4>5)  a second body voice, detuned, for width.
    //
    // Velocity goes mostly on the tine, so that playing harder opens the timbre
    // rather than only turning it up. That is the character of the instrument.
    mkPatch('Elec Piano 1', 4, 0, {
      0: { r:[99,76,42,60], l:[99,35,0,0], out:86, coarse:14, vel:7, krs:3, bp:43, rd:26, rc:0 },
      1: { r:[99,50,26,48], l:[99,94,74,0], out:95, coarse:1, vel:2, krs:3, bp:39, rd:22, rc:3 },
      2: { r:[98,62,46,58], l:[99,70,28,0], out:78, coarse:1, vel:5, krs:4, bp:43, rd:26, rc:0 },
      3: { r:[99,46,24,46], l:[99,95,78,0], out:98, coarse:1, vel:2, krs:3, bp:39, rd:18, rc:3 },
      4: { r:[98,66,48,60], l:[99,64,22,0], out:72, coarse:1, vel:4, krs:4, detune:10, bp:43, rd:26, rc:0 },
      5: { r:[99,48,25,47], l:[99,94,76,0], out:89, coarse:1, vel:2, krs:3, detune:4 },
    }),
    // The brighter one: a harder tine that lasts longer, a second high partial
    // above it, and a body that sustains rather than decaying away.
    mkPatch('Elec Piano 2', 4, 0, {
      0: { r:[99,68,40,58], l:[99,48,0,0], out:92, coarse:14, vel:7, krs:3, bp:43, rd:26, rc:0 },
      1: { r:[99,52,28,50], l:[99,94,72,0], out:94, coarse:1, vel:2, krs:3, bp:39, rd:26, rc:3 },
      2: { r:[99,72,44,60], l:[99,42,0,0], out:80, coarse:11, vel:6, krs:3, bp:43, rd:26, rc:0 },
      3: { r:[99,48,26,48], l:[99,95,76,0], out:97, coarse:1, vel:2, krs:3, bp:39, rd:22, rc:3 },
      4: { r:[98,64,46,58], l:[99,60,20,0], out:74, coarse:1, vel:5, krs:4, detune:9, bp:43, rd:26, rc:0 },
      5: { r:[99,50,27,49], l:[99,94,74,0], out:88, coarse:1, vel:2, krs:3, detune:5 },
    }),
    // Algo 1 chains: OP6→OP5→OP4→OP3(carrier), OP2→OP1(carrier), fb on OP6.
    // ops[] is Dexed order (ops[5]=OP1), so the 2-op sounds live at 5 (carrier)
    // and 4 (modulator); a third op at 3 is the second carrier.
    // Algorithm 1 has two carriers, OP1 and OP3, and this patch was using only
    // one of them. With its single carrier already at 99 there was no way to
    // bring it up to the rest of the bank. The second chain gives it a sub
    // octave underneath, which is what a bass patch wants anyway.
    mkPatch('FM Bass', 0, 6, {
      2: { r:[99,86,72,88], l:[99,62,0,0], out:74, coarse:2, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[99,80,68,84], l:[99,94,82,0], out:99, coarse:1, vel:2, detune:9 },
      4: { r:[99,88,75,90], l:[99,70,0,0], out:88, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[99,82,70,85], l:[99,95,85,0], out:99, coarse:1, vel:2 },
    }),
    mkPatch('Synth Bass', 0, 7, {
      3: { r:[99,90,78,92], l:[99,65,0,0], out:95, coarse:2, vel:3 },
      4: { r:[99,86,72,88], l:[99,78,0,0], out:87, coarse:1, vel:4, bp:43, rd:26, rc:0 },
      5: { r:[99,80,68,82], l:[99,96,88,0], out:99, coarse:1, vel:2 },
    }),
    mkPatch('Bright Bell', 4, 0, {
      0: { r:[99,62,42,50], l:[99,60,0,0], out:82, coarse:3, fine:50, vel:6, bp:43, rd:26, rc:0 },
      1: { r:[99,50,35,40], l:[99,95,80,0], out:98, coarse:1, vel:2 },
      2: { r:[99,66,46,54], l:[99,55,0,0], out:78, coarse:5, fine:25, vel:6, bp:43, rd:26, rc:0 },
      3: { r:[99,48,33,38], l:[99,96,82,0], out:92, coarse:1, vel:2 },
      4: { r:[99,70,50,58], l:[99,50,0,0], out:72, coarse:7, fine:75, vel:6, bp:43, rd:26, rc:0 },
      5: { r:[99,45,30,35], l:[99,97,84,0], out:87, coarse:1, vel:2 },
    }),
    mkPatch('Tubular Bell', 4, 2, {
      0: { r:[99,52,32,42], l:[99,65,25,0], out:80, coarse:3, fine:52, vel:6, bp:43, rd:26, rc:0 },
      1: { r:[99,38,22,30], l:[99,97,88,0], out:94, coarse:1, vel:2 },
      2: { r:[99,56,36,46], l:[99,60,20,0], out:76, coarse:7, fine:10, vel:6, bp:43, rd:26, rc:0 },
      3: { r:[99,36,20,28], l:[99,97,90,0], out:89, coarse:1, vel:2 },
      4: { r:[99,60,40,50], l:[99,55,15,0], out:72, coarse:4, fine:30, vel:6, bp:43, rd:26, rc:0 },
      5: { r:[99,34,18,26], l:[99,98,92,0], out:83, coarse:2, vel:2 },
    }),
    // Algo 22: OP2→OP1, OP6→OP3/OP4/OP5, carriers OP1,3,4,5 (ops 5,3,2,1),
    // modulators OP2 (ops[4]) and OP6 (ops[0], feedback).
    mkPatch('FM Brass', 21, 7, {
      0: { r:[72,60,50,70], l:[99,80,70,0], out:70, coarse:1, vel:5, bp:43, rd:26, rc:0 },
      1: { r:[62,50,50,60], l:[99,90,90,0], out:82, coarse:1, vel:3 },
      2: { r:[62,50,50,60], l:[99,90,90,0], out:72, coarse:1, vel:3 },
      3: { r:[62,50,50,60], l:[99,90,90,0], out:87, coarse:1, vel:3 },
      4: { r:[72,60,50,70], l:[99,80,70,0], out:80, coarse:1, vel:5, bp:43, rd:26, rc:0 },
      5: { r:[62,50,50,60], l:[99,90,90,0], out:96, coarse:1, vel:3 },
    }),
    mkPatch('Soft Brass', 21, 5, {
      0: { r:[65,55,45,65], l:[99,75,65,0], out:65, coarse:1, vel:4, bp:43, rd:26, rc:0 },
      1: { r:[55,45,45,55], l:[99,92,92,0], out:85, coarse:1, detune:6, vel:2 },
      2: { r:[55,45,45,55], l:[99,92,92,0], out:65, coarse:1, vel:2 },
      3: { r:[55,45,45,55], l:[99,92,92,0], out:89, coarse:1, detune:8, vel:2 },
      4: { r:[65,55,45,65], l:[99,75,65,0], out:72, coarse:1, vel:4, bp:43, rd:26, rc:0 },
      5: { r:[55,45,45,55], l:[99,92,92,0], out:96, coarse:1, vel:2 },
    }),
    // Algo 2: OP2→OP1(carrier, fb on OP2), OP6→OP5→OP4→OP3(carrier).
    // Carriers OP1/OP3 are ops[5]/ops[3]; the deep-chain mods sit above OP3.
    mkPatch('String Pad', 1, 4, {
      0: { r:[60,40,40,60], l:[99,70,70,0], out:60, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[60,40,40,60], l:[99,70,70,0], out:60, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      2: { r:[60,40,40,60], l:[99,70,70,0], out:60, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[50,30,30,50], l:[99,95,95,0], out:88, coarse:1, detune:6, vel:1 },
      4: { r:[55,35,35,55], l:[99,80,80,0], out:70, coarse:1, detune:6, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[50,30,30,50], l:[99,95,95,0], out:97, coarse:1, detune:8, vel:1 },
    }),
    mkPatch('Warm Strings', 1, 3, {
      0: { r:[55,38,38,58], l:[99,68,68,0], out:55, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[55,38,38,58], l:[99,68,68,0], out:55, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      2: { r:[55,38,38,58], l:[99,68,68,0], out:55, coarse:2, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[45,28,28,48], l:[99,96,96,0], out:90, coarse:1, detune:6, vel:1 },
      4: { r:[50,32,32,52], l:[99,82,82,0], out:65, coarse:1, detune:5, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[45,28,28,48], l:[99,96,96,0], out:97, coarse:1, detune:9, vel:1 },
    }),
    mkPatch('Drawbar Organ', 31, 0, {
      0: { r:[99,99,99,99], l:[99,99,99,0], out:93, coarse:1 },
      1: { r:[99,99,99,99], l:[99,99,99,0], out:84, coarse:2 },
      2: { r:[99,99,99,99], l:[99,99,99,0], out:79, coarse:3 },
      3: { r:[99,99,99,99], l:[99,99,99,0], out:74, coarse:4 },
      4: { r:[99,99,99,99], l:[99,99,99,0], out:69, coarse:6 },
      5: { r:[99,99,99,99], l:[99,99,99,0], out:64, coarse:8 },
    }),
    mkPatch('Perc Organ', 31, 2, {
      0: { r:[99,99,99,99], l:[99,99,99,0], out:94, coarse:1 },
      1: { r:[99,99,99,99], l:[99,99,99,0], out:83, coarse:2 },
      2: { r:[99,70,50,80], l:[99,60,0,0], out:85, coarse:3, vel:6 },
      3: { r:[99,99,99,99], l:[99,99,99,0], out:77, coarse:4 },
      4: { r:[99,80,60,85], l:[99,50,0,0], out:77, coarse:6, vel:5 },
      5: { r:[99,99,99,99], l:[99,99,99,0], out:60, coarse:8 },
    }),
    mkPatch('Pluck Key', 4, 5, {
      0: { r:[99,80,50,85], l:[99,50,0,0], out:80, coarse:3, bp:43, rd:26, rc:0 },
      1: { r:[99,70,40,80], l:[99,60,0,0], out:99, coarse:2, vel:5 },
    }),
    mkPatch('Mallet Hit', 4, 0, {
      0: { r:[99,82,55,88], l:[99,26,0,0], out:72, coarse:4, vel:7, bp:43, rd:26, rc:0 },
      1: { r:[99,52,24,70], l:[99,82,46,0], out:99, coarse:1, vel:3, krs:2 },
      2: { r:[99,84,57,88], l:[99,22,0,0], out:66, coarse:7, vel:6, bp:43, rd:26, rc:0 },
      3: { r:[99,54,26,72], l:[99,80,42,0], out:99, coarse:1, vel:3, krs:2, detune:9 },
    }),
    mkPatch('Soft Mallet', 4, 0, {
      0: { r:[99,70,40,80], l:[99,40,0,0], out:60, coarse:3, vel:5, bp:43, rd:26, rc:0 },
      1: { r:[99,35,20,50], l:[99,80,0,0], out:99, coarse:1, vel:2 },
      3: { r:[99,35,20,50], l:[99,80,0,0], out:85, coarse:1, vel:2 },
    }, { lfoSpeed:40, lfoAmpModDepth:30 }),
    mkPatch('Tremolo Bell', 4, 1, {
      0: { r:[99,60,30,70], l:[99,55,0,0], out:68, coarse:5, vel:5, bp:43, rd:26, rc:0 },
      1: { r:[99,25,12,40], l:[99,88,0,0], out:98, coarse:1, ams:2, vel:2 },
      2: { r:[99,65,35,72], l:[99,50,0,0], out:62, coarse:8, vel:5, bp:43, rd:26, rc:0 },
      3: { r:[99,25,12,40], l:[99,88,0,0], out:87, coarse:1, ams:2, vel:2 },
    }, { lfoSpeed:45, lfoAmpModDepth:40, lfoWave:0 }),
    mkPatch('Flute Tone', 0, 3, {
      4: { r:[75,50,45,60], l:[99,60,55,0], out:55, coarse:1, vel:4, bp:43, rd:26, rc:0 },
      5: { r:[70,40,40,55], l:[99,92,92,0], out:99, coarse:1, vel:2 },
    }, { lfoSpeed:38, lfoPitchModDepth:8, pitchModSens:3 }),
    mkPatch('Reed Pipe', 0, 5, {
      3: { r:[70,48,42,62], l:[99,65,60,0], out:60, coarse:3, vel:2 },
      4: { r:[70,48,42,62], l:[99,72,68,0], out:75, coarse:2, vel:5, bp:43, rd:26, rc:0 },
      5: { r:[65,42,42,58], l:[99,90,90,0], out:99, coarse:1, vel:2 },
    }),
    mkPatch('Synth Lead', 0, 7, {
      3: { r:[85,60,50,70], l:[99,75,65,0], out:80, coarse:2, vel:2 },
      4: { r:[85,60,50,70], l:[99,80,70,0], out:82, coarse:1, vel:5, bp:43, rd:26, rc:0 },
      5: { r:[80,50,50,65], l:[99,90,90,0], out:99, coarse:1, vel:2 },
    }),
    mkPatch('Bright Lead', 0, 7, {
      2: { r:[90,70,55,78], l:[99,70,55,0], out:65, coarse:5, bp:43, rd:26, rc:0 },
      3: { r:[88,62,52,72], l:[99,78,68,0], out:94, coarse:3 },
      4: { r:[88,62,52,72], l:[99,82,72,0], out:85, coarse:1, bp:43, rd:26, rc:0 },
      5: { r:[85,55,55,68], l:[99,88,88,0], out:99, coarse:1, vel:2 },
    }),
    mkPatch('Glass Pad', 4, 2, {
      0: { r:[50,30,28,50], l:[99,70,65,0], out:62, coarse:5, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[45,25,25,45], l:[99,95,95,0], out:95, coarse:1, vel:1 },
      2: { r:[50,30,28,50], l:[99,65,60,0], out:58, coarse:7, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[45,25,25,45], l:[99,95,95,0], out:86, coarse:1, detune:8, vel:1 },
      4: { r:[50,30,28,50], l:[99,60,55,0], out:52, coarse:3, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[45,25,25,45], l:[99,95,95,0], out:78, coarse:1, detune:6, vel:1 },
    }),
    mkPatch('Shimmer Pad', 4, 3, {
      0: { r:[45,28,25,48], l:[99,68,62,0], out:58, coarse:4, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[40,22,22,42], l:[99,96,96,0], out:97, coarse:1, ams:1, vel:1 },
      2: { r:[45,28,25,48], l:[99,62,56,0], out:52, coarse:6, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[40,22,22,42], l:[99,96,96,0], out:90, coarse:1, detune:9, ams:1, vel:1 },
      4: { r:[45,28,25,48], l:[99,58,50,0], out:48, coarse:3, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[40,22,22,42], l:[99,96,96,0], out:83, coarse:2, detune:5, vel:1 },
    }, { lfoSpeed:32, lfoAmpModDepth:20, lfoWave:0 }),
    mkPatch('Harpsichord', 0, 6, {
      2: { r:[99,80,50,88], l:[99,36,0,0], out:70, coarse:4, bp:43, rd:26, rc:0 },
      3: { r:[99,74,42,86], l:[99,44,0,0], out:99, coarse:3 },
      4: { r:[99,66,34,84], l:[99,56,18,0], out:86, coarse:2, krs:2, bp:43, rd:26, rc:0 },
      5: { r:[99,58,26,78], l:[99,84,50,0], out:99, coarse:1, vel:4, krs:2 },
    }),
    mkPatch('Clavinet', 4, 5, {
      0: { r:[99,76,44,85], l:[99,44,0,0], out:80, coarse:3, bp:43, rd:26, rc:0 },
      1: { r:[99,62,30,80], l:[99,78,40,0], out:99, coarse:2, vel:5, krs:3 },
      2: { r:[99,78,46,87], l:[99,42,0,0], out:75, coarse:5, bp:43, rd:26, rc:0 },
      3: { r:[99,64,32,82], l:[99,76,38,0], out:99, coarse:2, detune:8, krs:3 },
      4: { r:[99,80,48,86], l:[99,38,0,0], out:62, coarse:4, bp:43, rd:26, rc:0 },
      5: { r:[99,66,34,84], l:[99,74,36,0], out:99, coarse:1, vel:4, krs:3, detune:4 },
    }),
    mkPatch('Metallic Hit', 4, 3, {
      0: { r:[99,88,60,90], l:[99,40,0,0], out:78, coarse:6, fine:15, bp:43, rd:26, rc:0 },
      1: { r:[99,80,50,85], l:[99,45,0,0], out:99, coarse:1, vel:4 },
      2: { r:[99,90,65,92], l:[99,35,0,0], out:72, coarse:9, fine:25, bp:43, rd:26, rc:0 },
      3: { r:[99,82,52,87], l:[99,42,0,0], out:90, coarse:1 },
      4: { r:[99,85,58,88], l:[99,38,0,0], out:65, coarse:13, fine:10, bp:43, rd:26, rc:0 },
      5: { r:[99,78,48,83], l:[99,48,0,0], out:82, coarse:1 },
    }),
    mkPatch('Choir Pad', 1, 3, {
      0: { r:[48,30,28,50], l:[99,60,52,0], out:42, coarse:4, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[48,30,28,50], l:[99,65,58,0], out:48, coarse:3, vel:3, bp:43, rd:26, rc:0 },
      2: { r:[48,30,28,50], l:[99,68,62,0], out:52, coarse:2, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[42,25,25,45], l:[99,96,96,0], out:90, coarse:1, detune:6, vel:1 },
      4: { r:[48,30,28,50], l:[99,72,68,0], out:58, coarse:1, detune:6, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[42,25,25,45], l:[99,96,96,0], out:97, coarse:1, detune:8, vel:1 },
    }, { lfoSpeed:28, lfoPitchModDepth:5, pitchModSens:2 }),
    mkPatch('Deep Sub Bass', 0, 7, {
      4: { r:[99,45,28,72], l:[99,75,0,0], out:88, coarse:1, vel:4, bp:43, rd:26, rc:0 },
      5: { r:[99,35,20,65], l:[99,85,0,0], out:99, coarse:0, vel:2 },
    }),
    mkPatch('Pluck Bass', 0, 5, {
      3: { r:[99,80,50,85], l:[99,45,0,0], out:83, coarse:3 },
      4: { r:[99,75,45,82], l:[99,50,0,0], out:80, coarse:2, bp:43, rd:26, rc:0 },
      5: { r:[99,65,35,78], l:[99,55,0,0], out:99, coarse:1, vel:4 },
    }),
    mkPatch('Crystal Keys', 4, 1, {
      0: { r:[96,45,30,70], l:[99,60,0,0], out:65, coarse:4, bp:43, rd:26, rc:0 },
      1: { r:[97,28,18,55], l:[99,82,0,0], out:97, coarse:1, vel:3 },
      2: { r:[96,50,32,72], l:[99,55,0,0], out:58, coarse:6, bp:43, rd:26, rc:0 },
      3: { r:[97,28,18,55], l:[99,82,0,0], out:86, coarse:1, detune:8 },
      4: { r:[96,55,35,75], l:[99,50,0,0], out:52, coarse:8, bp:43, rd:26, rc:0 },
      5: { r:[97,25,15,50], l:[99,85,0,0], out:76, coarse:2 },
    }),
    mkPatch('Warm Pad', 1, 2, {
      0: { r:[42,25,22,45], l:[99,60,52,0], out:38, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      1: { r:[42,25,22,45], l:[99,65,58,0], out:42, coarse:1, vel:3, bp:43, rd:26, rc:0 },
      2: { r:[42,25,22,45], l:[99,70,65,0], out:48, coarse:2, vel:3, bp:43, rd:26, rc:0 },
      3: { r:[38,20,20,40], l:[99,97,97,0], out:92, coarse:1, detune:6, vel:1 },
      4: { r:[42,25,22,45], l:[99,75,72,0], out:55, coarse:1, detune:6, vel:3, bp:43, rd:26, rc:0 },
      5: { r:[38,20,20,40], l:[99,97,97,0], out:97, coarse:1, detune:8, vel:1 },
    }),
    mkPatch('Sync Lead', 0, 7, {
      0: { r:[94,75,60,80], l:[99,65,48,0], out:55, coarse:7, vel:5, bp:43, rd:26, rc:0 },
      1: { r:[92,70,58,78], l:[99,70,55,0], out:62, coarse:5, vel:5, bp:43, rd:26, rc:0 },
      2: { r:[92,70,58,78], l:[99,75,62,0], out:72, coarse:3, vel:5, bp:43, rd:26, rc:0 },
      3: { r:[90,65,55,75], l:[99,80,70,0], out:99, coarse:2, vel:2 },
      4: { r:[90,65,55,75], l:[99,84,74,0], out:88, coarse:1, vel:5, bp:43, rd:26, rc:0 },
      5: { r:[88,58,58,70], l:[99,86,86,0], out:99, coarse:1, vel:2 },
    }, { oscSync:true }),
    // INIT voice: algorithm 1 with OP1 (ops[5]) as the sole carrier, the blank
    // sheet you start a patch from. A bare sine at full output is some 8 dB
    // hotter than anything else here, which is a nasty surprise when scrolling
    // through sounds, so the copy that ships is trimmed to sit with its
    // neighbours. createDefaultPatch() itself is untouched, so an INIT you
    // build on still starts with all of its headroom.
    (() => { const p = createDefaultPatch(); p.ops[5].outputLevel = 94; return p; })(),
  ];
}
