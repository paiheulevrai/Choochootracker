#ifndef __CHIPNOMAD_LIB__EXPORT_H__
#define __CHIPNOMAD_LIB__EXPORT_H__

#include <stdint.h>
#include <stdio.h>

#include "project.h"
#include "chipnomad_lib.h"

// Exporter base class
class Exporter {
  protected:
    ChipNomadState* chipnomadState;
    int renderedSeconds;

  public:
    Exporter(Project* project, int startRow) {
      this->chipnomadState = chipnomadCreate();
      this->chipnomadState->project = *project;
      this->chipnomadState->ownsProjectResources = 0;
      this->renderedSeconds = 0;
      playbackInit(&this->chipnomadState->playbackState, &this->chipnomadState->project);
      playbackStartSong(&this->chipnomadState->playbackState, startRow, 0, 0);
    };

    virtual ~Exporter() {
      if (this->chipnomadState) {
        chipnomadDestroy(this->chipnomadState);
      }
    };

    void setMixVolume(float volume) { chipnomadState->mixVolume = volume; }

    virtual int next() = 0; // Returns seconds rendered, -1 if done
    virtual int finish() = 0;
    virtual void cancel() = 0;
};


// WAV Exporter
class ExporterWAV : public Exporter {
  private:
    FILE** files;        // Array of file handles (1 for normal, trackCount for stems)
    int fileCount;       // Number of output files
    int currentTrack;    // Current track being rendered (stems mode)
    int sampleRate;
    int channels;
    int bitDepth;
    int totalSamples;
    bool stems;           // false = single mixed file, true = one file per track
    char basePath[1024]; // Base path for file naming
    float* renderBuffer;

    void writeSamples(FILE* f, float* buffer, int samples);

  public:
    ExporterWAV(const char* path, Project* project, int startRow, int sampleRate, int bitDepth, float mixVolume, bool stems = false);
    ~ExporterWAV() override { cancel(); }
    int next() override;
    int finish() override;
    void cancel() override;
};

// Bounce selection: which region of the project to render
struct ExportSelection {
  int level; // 0 = song, 1 = chain, 2 = phrase
  int startSongRow, endSongRow;
  int startChainRow, endChainRow;
  int startPhraseRow, endPhraseRow;
  uint8_t trackMask; // Bit per track (song level); chain/phrase use one track
};

// Renders only a selected region (song rows / chain rows / phrase rows).
// Unselected tracks are muted (song level); playback stops (notes killed)
// when the region ends, so the file length matches the selection. Every
// selected track is included: at song level a track whose first chain is
// later in the region waits silently and joins when its song row arrives.
class ExporterSelectionWAV : public ExporterWAV {
  public:
    ExporterSelectionWAV(const char* path, Project* project, const ExportSelection& selection,
                         int sampleRate, int bitDepth, float mixVolume);
};

// Length of the audio a selection bounce will render, measured in phrase
// rows (16th notes): one phrase = 16 rows, one beat = 4 rows. Song level
// returns the longest selected track timeline (empty song rows keep a
// track waiting 16 rows each); chain level counts consecutive non-empty
// chain rows from the start row; phrase level is the selected row span.
int exportSelectionLengthRows(const Project* project, const ExportSelection& selection);


#endif // __CHIPNOMAD_LIB__EXPORT_H__
