#ifndef __TRACK_BOUNCE_H__
#define __TRACK_BOUNCE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Track bounce capture (Phase 7). The engine calls the two audioRecorder*
// hooks from its per-sample render path; this module stashes the samples in
// a lock-free SPSC ring that the recording writer thread drains. Everything
// here is allocation-free and lock-free so it is safe inside the audio
// callback.

// Begin capturing track `trackIdx` (0-based). Resets the ring.
void trackBounceStart(int trackIdx);
// Stop capturing. Keeps buffered data available for draining.
void trackBounceStop(void);
// Currently captured track, or -1.
int trackBounceActiveTrack(void);

// Engine hooks (called per sample from chipnomad_lib.cpp):
bool audioRecorderWantsTrackSample(int trackIdx);
void audioRecorderCaptureTrackSample(int trackIdx, int sampleIndex, float sample);
// Chunk bracket (engine thread, per render chunk): capture sites only run
// for active voices, so end-of-chunk fills the remainder with silence to
// keep the recording time-aligned.
void audioRecorderBeginTrackChunk(void);
void audioRecorderEndTrackChunk(int frames);

// Samples dropped because the ring was full (writer thread stalled).
uint64_t trackBounceDropped(void);

// Writer-thread side: pop up to maxStereoSamples interleaved stereo frames
// (L,R pairs, int16). Returns the number of stereo frames written, 0 when
// no complete pair is available. Odd leftovers are kept for the next call.
int trackBouncePop(int16_t* stereo, int maxStereoSamples);

// UI side: rolling decimated mono preview (-1..1) of the captured track.
// Returns the number of floats written to out (up to maxCount).
int trackBouncePreview(float* out, int maxCount);

#ifdef __cplusplus
}
#endif

#endif // __TRACK_BOUNCE_H__
