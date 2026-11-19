#include "doctest.h"
#include "piano_display.h"
#include "screens.h"
TEST_CASE("Pixel piano retains the reference key silhouettes and whole pixel blocks") {
  const int whiteX[] = {2, 6, 11, 15, 19, 24, 29};
  const int white[] = {0, 2, 4, 5, 7, 9, 11};
  for (int i = 0; i < 7; ++i)
    CHECK(monitorPianoPitchAt(whiteX[i], 7, 32, 9) == white[i]);
  const int blackX[] = {4, 9, 17, 22, 27}, black[] = {1, 3, 6, 8, 10};
  for (int i = 0; i < 5; ++i)
    CHECK(monitorPianoPitchAt(blackX[i], 2, 32, 9) == black[i]);
  CHECK(monitorPianoPitchAt(3, 3, 32, 9) == 1);
  CHECK(monitorPianoPitchAt(3, 5, 32, 9) == 0); // Stepped white key below C#.
  CHECK(monitorPianoPitchAt(4, 5, 32, 9) == -1); // Thick key separation.
  CHECK(monitorPianoPitchAt(13, 2, 32, 9) == -1); // E/F gap, no black key.
  CHECK(monitorPianoPitchAt(0, 2, 32, 9) == -1);
  CHECK(monitorPianoPitchAt(31, 2, 32, 9) == -1);
  CHECK(monitorPianoPitchAt(10, 8, 32, 9) == -1);
  // The pitch lookup maps whole 2x blocks within a centered 64x24 area.
  for (int y = 0; y < 9; ++y) for (int x = 0; x < 32; ++x)
    for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx)
      CHECK(monitorPianoPitchAt(x * 2 + sx, 3 + y * 2 + sy, 64, 24) ==
            monitorPianoPitchAt(x, y, 32, 9));
  CHECK(monitorPianoPitchAt(10, 2, 64, 24) == -1);
  CHECK(monitorPianoPitchAt(10, 21, 64, 24) == -1);
}

#include "chipnomad_lib.h"
#include "common.h"
TEST_CASE("Piano lights all sounding pitch classes, including chords, and respects mute and stop") {
  ChipNomadState* saved = chipnomadState;
  chipnomadState = chipnomadCreate();
  REQUIRE(chipnomadState != nullptr);
  chipnomadState->project.tracksCount = 3;
  // No audio producer runs in this test; seed the UI-facing status directly.
  auto& status = chipnomadState->uiPlaybackStatus;
  for (int i=0;i<3;++i) { status.trackEnabled[i]=1; status.tracks[i].note.pitchFinal=EMPTY_VALUE_8; }
  status.tracks[0].note.pitchFinal=48;
  status.tracks[0].chordVoiceCount=3;
  status.tracks[0].chordPitchFinal[0]=48;
  status.tracks[0].chordPitchFinal[1]=52;
  status.tracks[0].chordPitchFinal[2]=55;
  status.tracks[1].note.pitchFinal=61;
  CHECK(monitorPianoNotes() == ((1<<0)|(1<<4)|(1<<7)|(1<<1)));
  status.trackEnabled[0]=0;
  CHECK(monitorPianoNotes() == (1<<1));
  status.tracks[1].note.pitchFinal=NOTE_OFF;
  CHECK(monitorPianoNotes() == 0);
  status.trackEnabled[0]=1;
  status.tracks[0].note.pitchFinal=EMPTY_VALUE_8;
  CHECK(monitorPianoNotes() == 0); // Stale chord slots do not light after stop.
  chipnomadDestroy(chipnomadState);
  chipnomadState=saved;
}
