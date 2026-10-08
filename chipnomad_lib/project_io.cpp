#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "project.h"
#include "opll_presets.h"
#include "opl_patch.h"
#include "four_op_patch.h"
#include "simple_chip_presets.h"
#include <memory>
#include "project_io_common.h"
#include "synth/sample_voice.h"
#include "synth/sr_wavetable_loader.h"
#include "utils.h"

// Shared state
char projectFileError[41];
int projectFileVersion = 7;  // Default to current version
static char chipNames[][16] = { "AY8910" };

// Peek/consume implementation - single global buffer (ChipNomad is single-threaded)
static char lineBuffer[1024];
static char* currentLine = NULL;
static int isConsumed = 1;

static uint16_t scanPhraseVolume(char* str);
static uint16_t legacyPhraseVolume(uint16_t value);

void resetPeekConsume(void) {
  currentLine = NULL;
  isConsumed = 1;
}

// Helper: Read and trim a line from file
static char* readAndTrimLine(FILE* file) {
  char* result = fgets(lineBuffer, sizeof(lineBuffer), file);
  if (result == NULL) return NULL;

  // Trim trailing whitespace
  int idx = strlen(lineBuffer) - 1;
  while (idx >= 0 && isspace((unsigned char)lineBuffer[idx])) {
    lineBuffer[idx] = 0;
    idx--;
  }

  return lineBuffer;
}

char* peekLine(FILE* file) {
  // If current line is consumed, read next non-empty line
  if (isConsumed) {
    while (1) {
      currentLine = readAndTrimLine(file);
      if (currentLine == NULL) {
        snprintf(projectFileError, 40, "Unexpected end of file");
        return NULL;
      }
      // Skip empty lines
      if (strlen(currentLine) > 0) {
        isConsumed = 0;
        break;
      }
    }
  }

  return currentLine;
}

void consumeLine(FILE* file) {
  (void)file;  // Unused in single-buffer implementation
  isConsumed = 1;
}

// Binary data encoding/decoding functions
// 6-bit encoding character set (64 printable ASCII characters)
static const char* encodeTable = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";

// Save binary data with 6-bit text encoding
// Encodes 3 bytes into 4 characters, 80 chars per line = 60 bytes per line
// Note: Caller is responsible for writing the section header (#### Section Name)
int saveBinaryData(FILE* file, const uint8_t* data, uint16_t dataLen) {
  if (dataLen == 0 || data == NULL) {
    return 0;  // Nothing to save
  }

  // Write data header (length and data marker)
  fprintf(file, "- Length: %04X\n", dataLen);
  fprintf(file, "- Data:\n");

  char lineBuffer[81]; // 80 chars + null terminator
  int linePos = 0;

  // Encode 3 bytes into 4 characters (6 bits each)
  for (uint16_t i = 0; i < dataLen; i += 3) {
    uint32_t triple = 0;
    int bytesInGroup = 0;

    // Read up to 3 bytes
    for (int j = 0; j < 3 && (i + j) < dataLen; j++) {
      triple = (triple << 8) | data[i + j];
      bytesInGroup++;
    }

    // Pad with zeros if less than 3 bytes
    triple <<= (3 - bytesInGroup) * 8;

    // Extract 4 6-bit values
    for (int j = 0; j < 4; j++) {
      lineBuffer[linePos++] = encodeTable[(triple >> (18 - j * 6)) & 0x3F];

      // Write line when it reaches 80 characters
      if (linePos == 80) {
        lineBuffer[linePos] = '\0';
        fprintf(file, "%s\n", lineBuffer);
        linePos = 0;
      }
    }
  }

  // Write remaining characters
  if (linePos > 0) {
    lineBuffer[linePos] = '\0';
    fprintf(file, "%s\n", lineBuffer);
  }

  return 0;
}

// Load binary data from a #### section
// Expects to be called when "- Data:" line has been consumed
// Returns allocated buffer in outData (caller must free), length in outLen
int loadBinaryData(FILE* file, uint8_t** outData, uint16_t* outLen, uint16_t maxLen) {
  // Initialize decode table (static, initialized once)
  static int8_t decodeTable[256];
  static int decodeTableInitialized = 0;

  if (!decodeTableInitialized) {
    memset(decodeTable, -1, sizeof(decodeTable));
    for (int i = 0; i < 64; i++) {
      decodeTable[(uint8_t)encodeTable[i]] = i;
    }
    decodeTableInitialized = 1;
  }

  if (*outLen == 0 || *outLen > maxLen) {
    snprintf(projectFileError, 40, "Invalid data length");
    return 1;
  }

  // Allocate buffer
  uint8_t* buffer = (uint8_t*)malloc(*outLen);
  if (buffer == NULL) {
    snprintf(projectFileError, 40, "Memory allocation failed");
    return 1;
  }

  uint16_t bufferPos = 0;

  // Read encoded data lines
  while (bufferPos < *outLen) {
    char* line = peekLine(file);
    if (line == NULL) break;

    // Stop if we hit a section header or end of data
    if (line[0] == '#' || line[0] == '-') {
      break;
    }

    // Decode line (each 4 characters encode 3 bytes)
    int lineLen = strlen(line);
    for (int i = 0; i < lineLen && bufferPos < *outLen; i += 4) {
      // Read 4 6-bit values
      int8_t v[4] = {0, 0, 0, 0};
      int validChars = 0;
      for (int j = 0; j < 4 && (i + j) < lineLen; j++) {
        v[j] = decodeTable[(uint8_t)line[i + j]];
        if (v[j] >= 0) validChars++;
      }

      // Decode to 3 bytes if we have valid characters
      if (validChars >= 2) {
        uint32_t triple = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) |
                         ((uint32_t)v[2] << 6) | (uint32_t)v[3];

        // Extract bytes
        if (bufferPos < *outLen) {
          buffer[bufferPos++] = (triple >> 16) & 0xFF;
        }
        if (bufferPos < *outLen && validChars >= 3) {
          buffer[bufferPos++] = (triple >> 8) & 0xFF;
        }
        if (bufferPos < *outLen && validChars >= 4) {
          buffer[bufferPos++] = triple & 0xFF;
        }
      }
    }

    consumeLine(file);
  }

  *outData = buffer;
  return 0;
}

// Helper functions for parsing
static uint8_t scanByteOrEmpty(char* str) {
  static char buf[3];
  buf[0] = str[0];
  buf[1] = str[1];
  buf[2] = 0;
  if (buf[0] == '-' && buf[1] == '-') {
    return EMPTY_VALUE_8;
  } else {
    uint8_t result;
    if (sscanf(buf, "%hhX", &result) != 1) return EMPTY_VALUE_8;
    return result;
  }
}

static uint8_t scanNote(char* str, Project* p) {
  // Silly linear search through pitch table. To replace with a simple hash
  static char buf[4];
  buf[0] = str[0];
  buf[1] = str[1];
  buf[2] = str[2];
  buf[3] = 0;

  if (!strcmp(buf, "---")) return EMPTY_VALUE_8;
  if (!strcmp(buf, "OFF")) return NOTE_OFF;

  for (int c = 0; c < p->pitchTable.length; c++) {
    if (!strcmp(buf, p->pitchTable.noteNames[c])) return c;
  }

  return EMPTY_VALUE_8;
}

static uint8_t scanFX(char* str, Project* p) {
  // Silly linear search through the list of FX. To replace with a simple hash
  static char buf[4];
  buf[0] = str[0];
  buf[1] = str[1];
  buf[2] = str[2];
  buf[3] = 0;

  if (!strcmp(buf, "---")) return EMPTY_VALUE_8;

  // Legacy file spelling of the sample playback FX (shown as SPL in the
  // UI). The AY2 Pulse Low Level FX owns the "SPL" name in this flat
  // namespace, so the sample FX keeps "SLP" in files and resolves here.
  if (!strcmp(buf, "SLP")) return fxSLP;

  // Scan all FX groups
  extern FXGroup fxGroups[];
  extern int fxGroupCount;
  for (int g = 0; g < fxGroupCount; g++) {
    for (int c = 0; c < fxGroups[g].count; c++) {
      if (!strcmp(buf, fxGroups[g].fxList[c].name)) return fxGroups[g].fxList[c].fx;
    }
  }

  return EMPTY_VALUE_8;
}

///////////////////////////////////////////////////////////////////////////////
// Load functions

static int projectLoadPitchTable(FILE* file, Project* p) {
  char buf[128];

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Pitch table")) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  if (sscanf(line, "- Title: %18[^\n]", p->pitchTable.name) != 1) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  consumeLine(file);  // Skip opening ```

  int idx = 0;
  int period;
  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (sscanf(line, "%127s %d", buf, &period) != 2) {
      // Couldn't parse - must be closing ``` or next section
      break;
    }
    if (strlen(buf) != 3) return 1;
    if (idx >= PROJECT_MAX_PITCHES) return 1;
    strcpy(p->pitchTable.noteNames[idx], buf);
    p->pitchTable.values[idx] = period;
    idx++;
    consumeLine(file);
  }
  p->pitchTable.length = idx;

  // Calculate octave size (find first note name change)
  if (idx > 0) {
    char firstOctave = p->pitchTable.noteNames[0][2];
    p->pitchTable.octaveSize = 12; // Default
    for (int i = 1; i < idx; i++) {
      if (p->pitchTable.noteNames[i][2] != firstOctave) {
        p->pitchTable.octaveSize = i;
        break;
      }
    }
  }

  // Consume the closing ``` if present
  if (line[0] == '`') {
    consumeLine(file);
  }

  return 0;
}

static int projectLoadSong(FILE* file, Project* p) {
  char buf[4];
  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Song")) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  consumeLine(file);  // Skip opening ```

  int idx = 0;
  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#' || line[0] == '`') break;  // Next section or closing ```
    // Minimum length: 2 chars per cell, plus 1 separator for each cell (optional for last)
    if (strlen(line) < (p->tracksCount * 2 + p->tracksCount - 1)) return 1;

    for (int c = 0; c < p->tracksCount; c++) {
      buf[0] = line[c * 3];
      buf[1] = line[c * 3 + 1];
      buf[2] = 0;
      if (buf[0] == '-' && buf[1] == '-') {
        p->song[idx][c] = EMPTY_VALUE_16;
      } else {
        uint16_t result;
        if (sscanf(buf, "%hX", &result) != 1) return 1;
        p->song[idx][c] = result;
      }
      // Check for highlight marker
      if (line[c * 3 + 2] == '*') {
        p->songHighlight[idx][c] = 1;
      } else {
        p->songHighlight[idx][c] = 0;
      }
    }
    idx++;
    consumeLine(file);
  }

  // Consume closing ``` if present
  if (line[0] == '`') {
    consumeLine(file);
  }

  return 0;
}

static int projectLoadChains(FILE* file, Project* p) {
  int idx;

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Chains")) return 1;
  consumeLine(file);

  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strncmp(line, "### Chain", 9)) break;
    if (sscanf(line, "### Chain %X", &idx) != 1) return 1;
    consumeLine(file);

    // Skip opening ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }

    for (int c = 0; c < 16; c++) {
      line = peekLine(file);
      if (line == NULL) return 1;
      if (strlen(line) != 6) return 1;

      if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
        p->chains[idx].rows[c].phrase = EMPTY_VALUE_16;
      } else {
        uint16_t result;
        if (sscanf(line, "%hX", &result) != 1) return 1;
        p->chains[idx].rows[c].phrase = result;
      }
      p->chains[idx].rows[c].transpose = scanByteOrEmpty(line + 4);
      consumeLine(file);
    }

    // Skip closing ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }
  }

  return 0;
}

static int projectLoadGrooves(FILE* file, Project* p) {
  int idx;

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Grooves")) return 1;
  consumeLine(file);

  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strncmp(line, "### Groove", 10)) break;
    if (sscanf(line, "### Groove %X", &idx) != 1) return 1;
    consumeLine(file);

    // Skip opening ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }

    for (int c = 0; c < 16; c++) {
      line = peekLine(file);
      if (line == NULL) return 1;
      if (strlen(line) != 2) return 1;
      p->grooves[idx].speed[c] = scanByteOrEmpty(line);
      consumeLine(file);
    }

    // Skip closing ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }
  }

  return 0;
}

static int projectLoadPhrases(FILE* file, Project* p) {
  int idx;

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Phrases")) return 1;
  consumeLine(file);

  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strncmp(line, "### Phrase", 10)) break;
    if (sscanf(line, "### Phrase %X", &idx) != 1) return 1;
    consumeLine(file);

    // Skip opening ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }

    for (int c = 0; c < 16; c++) {
      line = peekLine(file);
      if (line == NULL) return 1;
      // Empty FX names are serialized as blank fields when the FX name table
      // has not been initialized yet, so the shortest valid phrase row is 21
      // characters instead of the usual 30.
      if (strlen(line) < 21) return 1;
      // Note
      p->phrases[idx].rows[c].note = scanNote(line, p);
      // Instrument
      p->phrases[idx].rows[c].instrument = scanByteOrEmpty(line + 4);
      // Volume
      p->phrases[idx].rows[c].volume = scanPhraseVolume(line + 7);
      // FX
      for (int d = 0; d < 3; d++) {
        p->phrases[idx].rows[c].fx[d][0] = scanFX(line + 10 + d * 7, p);
        p->phrases[idx].rows[c].fx[d][1] = scanByteOrEmpty(line + 14 + d * 7);
      }
      consumeLine(file);
    }

    // Skip closing ```
    line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '`') {
      consumeLine(file);
    }
  }

  return 0;
}

static int projectLoadInstruments(FILE* file, Project* p) {
  int idx;

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Instruments")) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  while (strncmp(line, "### Instrument", 14) == 0) {
    if (sscanf(line, "### Instrument %X", &idx) != 1) return 1;
    consumeLine(file);
    if (instrumentLoadData(file, &p->instruments[idx], p)) return 1;
    line = peekLine(file);
    if (line == NULL) return 1;
  }

  return 0;
}

static TableRetriggerMode tableRetriggerModeFromHeader(const char* line) {
  const char* value = strstr(line, "Retrig: ");
  if (!value) return TableRetriggerMode::instrument;
  value += 8;
  if (!strncmp(value, "Phrase", 6)) return TableRetriggerMode::phrase;
  if (!strncmp(value, "Chain", 5)) return TableRetriggerMode::chain;
  if (!strncmp(value, "Free", 4)) return TableRetriggerMode::free;
  return TableRetriggerMode::instrument;
}

static const char* tableRetriggerModeName(TableRetriggerMode mode) {
  switch (mode) {
    case TableRetriggerMode::phrase: return "Phrase";
    case TableRetriggerMode::chain: return "Chain";
    case TableRetriggerMode::free: return "Free";
    default: return "Inst";
  }
}

static int loadTable(FILE* file, Table* table, Project* p) {
  // Skip opening ```
  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (line[0] == '`') {
    consumeLine(file);
  }

  for (int d = 0; d < 16; d++) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strlen(line) < 35) return 1;  // Minimum length check

    // Pitch flag
    table->rows[d].pitchFlag = (line[0] == '=') ? 1 : 0;
    // Pitch offset
    table->rows[d].pitchOffset = scanByteOrEmpty(line + 2);
    // Volume
    table->rows[d].volume = scanByteOrEmpty(line + 5);
    // FX
    for (int e = 0; e < 4; e++) {
      table->rows[d].fx[e][0] = scanFX(line + 8 + e * 7, p);
      table->rows[d].fx[e][1] = scanByteOrEmpty(line + 12 + e * 7);
    }
    consumeLine(file);
  }

  // Skip closing ```
  line = peekLine(file);
  if (line == NULL) return 1;
  if (line[0] == '`') {
    consumeLine(file);
  }

  return 0;
}

static int projectLoadTables(FILE* file, Project* p) {
  int idx;

  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strcmp(line, "## Tables")) return 1;
  consumeLine(file);

  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strncmp(line, "### Table", 9)) break;
    if (sscanf(line, "### Table %X", &idx) != 1) return 1;
    p->tables[idx].retriggerMode = tableRetriggerModeFromHeader(line);
    consumeLine(file);
    if (loadTable(file, &p->tables[idx], p)) return 1;
  }

  return 0;
}

static int projectLoadAYWavetables(FILE* file, Project* p) {
  char* line = peekLine(file);
  if (line == NULL) return 1;

  // This section is optional for backwards compatibility
  if (strcmp(line, "## AY Wavetables")) {
    // Section not found, that's OK - just return success
    return 0;
  }
  consumeLine(file);

  // Read wavetable data lines until we hit EOF or another section
  while (1) {
    line = peekLine(file);
    if (line == NULL) return 1;

    // Check if we've reached EOF or another section marker
    if (!strcmp(line, "EOF") || !strncmp(line, "##", 2)) {
      break;
    }

    // Parse wavetable line: "XX 0123456789ABCDEF0123456789ABCDEF"
    int wavetableIdx;
    char data[33];  // 32 hex digits + null terminator

    if (sscanf(line, "%X %32s", &wavetableIdx, data) != 2) {
      snprintf(projectFileError, 40, "Invalid wavetable format");
      return 1;
    }

    // Validate wavetable index
    if (wavetableIdx < 0 || wavetableIdx > 255) {
      snprintf(projectFileError, 40, "Invalid wavetable index");
      return 1;
    }

    // Validate data length
    if (strlen(data) != 32) {
      snprintf(projectFileError, 40, "Invalid wavetable data length");
      return 1;
    }

    // Parse hex digits and store in wavetable
    for (int i = 0; i < 32; i++) {
      char c = data[i];
      uint8_t value;

      if (c >= '0' && c <= '9') {
        value = c - '0';
      } else if (c >= 'A' && c <= 'F') {
        value = c - 'A' + 10;
      } else if (c >= 'a' && c <= 'f') {
        value = c - 'a' + 10;
      } else {
        snprintf(projectFileError, 40, "Invalid hex digit in wavetable");
        return 1;
      }

      p->ayWavetables[wavetableIdx][i] = value & 0x0F;
    }

    consumeLine(file);
  }

  return 0;
}

// Strict byte CSV reader for the optional insert section. Reject overflow,
// signs, trailing fields and truncated data before narrowing any value.
static bool readInsertFields(const char* line, const char* prefix, unsigned* values, int count) {
  size_t length = strlen(prefix);
  if (!line || strncmp(line, prefix, length)) return false;
  const char* cursor = line + length;
  for (int field = 0; field < count; ++field) {
    if (*cursor < '0' || *cursor > '9') return false;
    unsigned value = 0;
    do {
      value = value * 10 + (*cursor++ - '0');
      if (value > 255) return false;
    } while (*cursor >= '0' && *cursor <= '9');
    values[field] = value;
    if (field + 1 < count) { if (*cursor++ != ',') return false; }
  }
  return *cursor == 0;
}

static int projectLoadInternal(FILE* file, Project* project) {
  char buf[128];
  int tempLinearPitch;
  Project p;
  projectInit(&p);
  p.signedTrackSpeed = 0; // Absent from old files: retain the legacy SPD map.
  p.perceptualEffects = 0;

  snprintf(projectFileError, 40, "Module header");
  char* line = peekLine(file);
  if (line == NULL) return 1;
  const char* version = NULL;
  if (!strncmp(line, "# ChooChooTracker Module", 24)) version = line + 24;
  else if (!strncmp(line, "# ChipNomad Tracker Module", 26)) version = line + 26;
  if (!version) {
    snprintf(projectFileError, 40, "Incorrect module format");
    return 1;
  }

  // Detect version
  if (strlen(version) > 0) {
    if (strncmp(version, " 10.0", 5) == 0) {
      projectFileVersion = 10;
    } else if (strncmp(version, " 9.0", 4) == 0) {
      projectFileVersion = 9;
    } else if (strncmp(version, " 8.0", 4) == 0) {
      projectFileVersion = 8;
    } else if (strncmp(version, " 7.0", 4) == 0) {
      projectFileVersion = 7;
    } else if (strncmp(version, " 6.0", 4) == 0) {
      projectFileVersion = 6;
    } else if (strncmp(version, " 5.0", 4) == 0) {
      projectFileVersion = 5;
    } else if (strncmp(version, " 4.0", 4) == 0) {
      projectFileVersion = 4;
    } else if (strncmp(version, " 3.0", 4) == 0) {
      projectFileVersion = 3;
    } else if (strncmp(version, " 2.0", 4) == 0) {
      projectFileVersion = 2;
    } else if (strncmp(version, " 1.0", 4) == 0) {
      projectFileVersion = 1;
    } else {
      snprintf(projectFileError, 40, "Incorrect module version");
      return 1;
    }
  } else {
    // No version specified = 1.0 (legacy)
    projectFileVersion = 1;
  }
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  if (!strncmp(line, "- Title:", 8)) {
    if (sscanf(line, "- Title: %24[^\n]", p.title) != 1) {
      // Empty title is valid, just set it to empty string
      p.title[0] = 0;
    }
  } else {
    snprintf(projectFileError, 40, "Invalid title");
    return 1;
  }
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  if (!strncmp(line, "- Author:", 9)) {
    if (sscanf(line, "- Author: %24[^\n]", p.author) != 1) {
      p.author[0] = 0;
    }
  }
  consumeLine(file);

  snprintf(projectFileError, 40, "Invalid project settings");
  line = peekLine(file);
  if (line == NULL) return 1;
  if (sscanf(line, "- Frame rate: %f", &p.tickRate) != 1) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line == NULL) return 1;
  if (sscanf(line, "- Chips count: %d", &p.chipsCount) != 1) return 1;
  if (p.chipsCount < 1 || p.chipsCount > PROJECT_MAX_TRACKS) return 1;
  consumeLine(file);

  line = peekLine(file);
  if (line && strncmp(line, "- Track inserts: ", 17) == 0) {
    unsigned header[3];
    if (!readInsertFields(line, "- Track inserts: ", header, 3) ||
        header[0] != 1 || header[1] != PROJECT_MAX_TRACKS || header[2] != 2) return 1;
    consumeLine(file);
    for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) for (int slot = 0; slot < 2; ++slot) {
      unsigned fields[12];
      if (!readInsertFields(peekLine(file), "- Insert: ", fields, 12) ||
          fields[0] != (unsigned)t || fields[1] != (unsigned)slot ||
          fields[2] >= insertModuleCount || fields[3] > 1) return 1;
      auto& c = p.trackInserts[t][slot]; c.module = fields[2]; c.bypass = fields[3];
      for (int i = 0; i < 8; ++i) c.values[i] = insertClamp(c.module, i, fields[i + 4]);
      consumeLine(file);
    }
    line = peekLine(file);
  }
  if (line == NULL || strncmp(line, "- Track volumes: ", 17) != 0) return 1;
  if (sscanf(line + 17, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
      &p.trackVolume[0], &p.trackVolume[1], &p.trackVolume[2], &p.trackVolume[3],
      &p.trackVolume[4], &p.trackVolume[5], &p.trackVolume[6], &p.trackVolume[7]) != PROJECT_MAX_TRACKS) return 1;
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    if (p.trackVolume[i] > 100) p.trackVolume[i] = 100;
  }
  consumeLine(file);

  line = peekLine(file);
  if (line && strncmp(line, "- Track pans: ", 14) == 0) {
    if (sscanf(line + 14, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
        &p.trackPan[0], &p.trackPan[1], &p.trackPan[2], &p.trackPan[3],
        &p.trackPan[4], &p.trackPan[5], &p.trackPan[6], &p.trackPan[7]) != PROJECT_MAX_TRACKS) return 1;
    consumeLine(file);
  }

  line = peekLine(file);
  if (line && strncmp(line, "- Reverb sends: ", 16) == 0) {
    if (sscanf(line + 16, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
        &p.trackReverbSend[0], &p.trackReverbSend[1], &p.trackReverbSend[2], &p.trackReverbSend[3],
        &p.trackReverbSend[4], &p.trackReverbSend[5], &p.trackReverbSend[6], &p.trackReverbSend[7]) != PROJECT_MAX_TRACKS) return 1;
    consumeLine(file);
  }
  line = peekLine(file);
  if (line && strncmp(line, "- Delay sends: ", 15) == 0) {
    if (sscanf(line + 15, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
        &p.trackDelaySend[0], &p.trackDelaySend[1], &p.trackDelaySend[2], &p.trackDelaySend[3],
        &p.trackDelaySend[4], &p.trackDelaySend[5], &p.trackDelaySend[6], &p.trackDelaySend[7]) != PROJECT_MAX_TRACKS) return 1;
    consumeLine(file);
  }
  line = peekLine(file);
  if (line && strncmp(line, "- Track tilts: ", 15) == 0) {
    if (sscanf(line + 15, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
        &p.trackTilt[0], &p.trackTilt[1], &p.trackTilt[2], &p.trackTilt[3],
        &p.trackTilt[4], &p.trackTilt[5], &p.trackTilt[6], &p.trackTilt[7]) != PROJECT_MAX_TRACKS) return 1;
    consumeLine(file);
  }
  line = peekLine(file);
  if (line && sscanf(line, "- Reverb: %hhu,%hhu,%hhu,%hu", &p.reverbReturn, &p.reverbTime,
                     &p.reverbDamping, &p.reverbFilterCutoffHz) == 4) consumeLine(file);
  line = peekLine(file);
  if (line && sscanf(line, "- Delay: %hhu,%hhu,%hhu,%hhu,%hu", &p.delayReturn, &p.delayReverbSend,
                     &p.delayTicks, &p.delayFeedback, &p.delayFilterCutoffHz) == 5) {
    consumeLine(file);
  } else if (line && sscanf(line, "- Delay: %hhu,%hhu,%hhu,%hu", &p.delayReturn,
                            &p.delayTicks, &p.delayFeedback, &p.delayFilterCutoffHz) == 4) {
    p.delayReverbSend = 0;
    consumeLine(file);
  }
  line = peekLine(file);
  if (line && sscanf(line, "- Tilt pivot: %hu", &p.tiltPivotHz) == 1) consumeLine(file);
  line = peekLine(file);
  if (line && sscanf(line, "- Sample save choice: %hhu", &p.sampleSaveChoice) == 1) {
    consumeLine(file);
  }

  line = peekLine(file);
  if (line && sscanf(line, "- MIDI CC mappings: %d", &tempLinearPitch) == 1) {
    int count = tempLinearPitch < 0 ? 0 : tempLinearPitch;
    consumeLine(file);
    for (int i = 0; i < count; ++i) {
      unsigned slot, enabled, channel, cc, instrument, destination;
      line = peekLine(file);
      if (!line || sscanf(line, "- MIDI CC: %u,%u,%u,%u,%u,%u", &slot, &enabled, &channel,
                          &cc, &instrument, &destination) != 6) return 1;
      if (slot < PROJECT_MAX_MIDI_CC_MAPPINGS) {
        MidiCCMapping& m = p.midiCCMappings[slot];
        m.enabled = enabled != 0;
        m.channel = channel < 16 ? channel : 0;
        m.cc = cc < 128 ? cc : 0;
        m.instrument = instrument < PROJECT_MAX_INSTRUMENTS ? instrument : 0;
        m.destination = destination < 255 ? destination : 0;
      }
      consumeLine(file);
    }
    line = peekLine(file);
  }

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    if (p.trackReverbSend[i] > 100) p.trackReverbSend[i] = 100;
    if (p.trackDelaySend[i] > 100) p.trackDelaySend[i] = 100;
  }
  if (p.reverbReturn > 100) p.reverbReturn = 100;
  if (p.delayReturn > 100) p.delayReturn = 100;
  if (p.delayReverbSend > 100) p.delayReverbSend = 100;
  if (p.delayFeedback > 95) p.delayFeedback = 95;
  if (p.delayTicks == 0) p.delayTicks = 1;
  if (p.reverbFilterCutoffHz < 20) p.reverbFilterCutoffHz = 20;
  if (p.delayFilterCutoffHz < 20) p.delayFilterCutoffHz = 20;
  if (p.tiltPivotHz < 250) p.tiltPivotHz = 250;
  if (p.tiltPivotHz > 4000) p.tiltPivotHz = 4000;
  if (p.sampleSaveChoice > 2) p.sampleSaveChoice = 0;

  // Try to read linear pitch (optional for backwards compatibility)
  line = peekLine(file);
  if (line == NULL) return 1;
  if (sscanf(line, "- Linear pitch: %d", &tempLinearPitch) == 1) {
    p.linearPitch = (uint8_t)tempLinearPitch;
    consumeLine(file);
    line = peekLine(file);  // Read next line for chip type
    if (line == NULL) return 1;
  }
  if (line && sscanf(line, "- Signed track speed: %d", &tempLinearPitch) == 1) {
    p.signedTrackSpeed = tempLinearPitch != 0;
    consumeLine(file);
    line = peekLine(file);
    if (line == NULL) return 1;
  }
  if (line && sscanf(line, "- Perceptual effects: %d", &tempLinearPitch) == 1) {
    p.perceptualEffects = tempLinearPitch != 0;
    consumeLine(file);
    line = peekLine(file);
    if (line == NULL) return 1;
  }
  int scaleRoot, scalePreset;
  unsigned int scaleCustomMask, scaleTracksMask;
  int scaleMode = 0;
  // Try the current 6-field layout: apply,mode,root,preset,customMask,tracksMask.
  // Legacy lines have no mode field (4 or 5 fields), so a short parse means the
  // values sit in the legacy positions and must be re-read that way.
  int scaleFields = line ? sscanf(line, "- Scale: %d,%d,%d,%d,%u,%u", &tempLinearPitch, &scaleMode,
                                  &scaleRoot, &scalePreset, &scaleCustomMask, &scaleTracksMask) : 0;
  if (scaleFields > 0 && scaleFields < 6) {
    scaleFields = sscanf(line, "- Scale: %d,%d,%d,%u,%u", &tempLinearPitch, &scaleRoot,
                         &scalePreset, &scaleCustomMask, &scaleTracksMask);
    scaleMode = 0; // legacy files predate Note Lock: default Quantizer
  }
  if (scaleFields >= 4) {
    p.scaleApply = tempLinearPitch != 0;
    p.scaleMode = scaleMode != 0;
    p.scaleRoot = scaleRoot >= 0 && scaleRoot < 12 ? (uint8_t)scaleRoot : 0;
    p.scalePreset = scalePreset >= 0 && scalePreset < scalePresetCount ? (ScalePreset)scalePreset : scaleChromatic;
    p.scaleCustomMask = scaleCustomMask & 0x0fff;
    if (!p.scaleCustomMask) p.scaleCustomMask = 0x0fff;
    if (scaleFields >= 5) p.scaleTracksMask = scaleTracksMask & 0xff;
    consumeLine(file);
    line = peekLine(file);
    if (line == NULL) return 1;
  }
  // If linear pitch not found, line already contains the chip type line

  // Chip type
  if (sscanf(line, "- Chip type: %s", buf) != 1) {
    snprintf(projectFileError, 40, "Invalid chip type");
    return 1;
  }

  if (!strcmp(buf, "AY8910")) {
    p.chipType = ChipType::AY;
  } else {
    snprintf(projectFileError, 40, "Unknown chip type");
    return 1;
  }
  consumeLine(file);

  // Chip-specific settings
  switch (p.chipType) {
  case ChipType::AY:
    line = peekLine(file);
    if (line == NULL) return 1;
    if (sscanf(line, "- *AY8910* Clock: %d", &p.chipSetup.ay.clock) != 1) return 1;
    consumeLine(file);

    int tempIsYM;
    line = peekLine(file);
    if (line == NULL) return 1;
    if (sscanf(line, "- *AY8910* AY/YM: %d", &tempIsYM) != 1) return 1;
    p.chipSetup.ay.isYM = (uint8_t)tempIsYM;
    consumeLine(file);

    // TODO: Remove old pan logic for the first public release
    line = peekLine(file);
    if (line == NULL) return 1;
    if (strncmp(line, "- *AY8910* PanA:", 15) == 0) {
      // Old pan storage
      consumeLine(file);
      line = peekLine(file);  // Skip B
      if (line == NULL) return 1;
      consumeLine(file);
      line = peekLine(file);  // Skip C
      if (line == NULL) return 1;
      consumeLine(file);
      line = peekLine(file);  // Skip stereo mode
      if (line == NULL) return 1;
      consumeLine(file);
      // Default to ABC
      p.chipSetup.ay.stereoMode = StereoModeAY::ABC;
      p.chipSetup.ay.stereoSeparation = 100;
    } else {
      // New stereo mode storage
      if (strncmp(line, "- *AY8910* Stereo:", 17) == 0) {
        if (sscanf(line, "- *AY8910* Stereo: %s", buf) != 1) {
          snprintf(projectFileError, 40, "Invalid stereo mode");
          return 1;
        }
        if (!strcmp(buf, "ABC")) {
          p.chipSetup.ay.stereoMode = StereoModeAY::ABC;
        } else if (!strcmp(buf, "ACB")) {
          p.chipSetup.ay.stereoMode = StereoModeAY::ACB;
        } else if (!strcmp(buf, "BAC")) {
          p.chipSetup.ay.stereoMode = StereoModeAY::BAC;
        } else {
          snprintf(projectFileError, 40, "Unknown stereo mode");
          return 1;
        }
      } else {
        snprintf(projectFileError, 40, "Invalid stereo mode");
        return 1;
      }
      consumeLine(file);

      line = peekLine(file);
      if (line == NULL) return 1;
      if (sscanf(line, "- *AY8910* Stereo separation: %hhu", &p.chipSetup.ay.stereoSeparation) != 1) return 1;
      consumeLine(file);

      // PWM range (optional field for backwards compatibility)
      line = peekLine(file);
      if (line != NULL && strncmp(line, "- *AY8910* PWM range: ", 22) == 0) {
        if (sscanf(line, "- *AY8910* PWM range: %hhu", &p.chipSetup.ay.pwmFullRange) != 1) return 1;
        consumeLine(file);
      } else {
        // Default to 16-step mode for old files
        p.chipSetup.ay.pwmFullRange = 0;
      }
    }
    break;
  default:
    break;
  }

  p.tracksCount = projectGetTotalTracks(&p);

  snprintf(projectFileError, 40, "Invalid pitch table");
  if (projectLoadPitchTable(file, &p)) return 1;
  snprintf(projectFileError, 40, "Invalid song data");
  if (projectLoadSong(file, &p)) return 1;
  snprintf(projectFileError, 40, "Invalid chain data");
  if (projectLoadChains(file, &p)) return 1;
  snprintf(projectFileError, 40, "Invalid groove data");
  if (projectLoadGrooves(file, &p)) return 1;
  snprintf(projectFileError, 40, "Invalid phrase data");
  if (projectLoadPhrases(file, &p)) return 1;
  snprintf(projectFileError, 40, "Invalid instrument data");
  if (projectLoadInstruments(file, &p)) { projectFree(&p); return 1; }
  snprintf(projectFileError, 40, "Invalid table data");
  if (projectLoadTables(file, &p)) { projectFree(&p); return 1; }
  snprintf(projectFileError, 40, "Invalid wavetable data");
  if (projectLoadAYWavetables(file, &p)) { projectFree(&p); return 1; }

  if (projectFileVersion < 5) {
    auto oldCents = [](uint8_t value) {
      if (!value) return 0;
      if (value <= 127) return (int)(pow(200.0, (value - 1) / 126.0) + 0.5);
      return ((int)value - 125) * 100;
    };
    auto newControl = [&](int cents) {
      cents = cents > 2400 ? 2400 : cents;
      if (cents <= 100) return (uint8_t)((cents + 2) / 5);
      return (uint8_t)(20 + ((cents - 100) * 235 + 1150) / 2300);
    };
    auto convert = [&](uint8_t value) { return newControl(oldCents(value > 149 ? 149 : value)); };
    for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
      Instrument* inst = &p.instruments[i];
      if (inst->type == InstrumentType::SCWF) inst->chip.scwf.detune = convert(inst->chip.scwf.detune);
      else if (inst->type == InstrumentType::BYOWTBL) inst->chip.byowtbl.detune = convert(inst->chip.byowtbl.detune);
    }
    for (int phrase = 0; phrase < PROJECT_MAX_PHRASES; ++phrase) for (int row = 0; row < 16; ++row) for (int fx = 0; fx < 3; ++fx)
      if (p.phrases[phrase].rows[row].fx[fx][0] == fxSDT) p.phrases[phrase].rows[row].fx[fx][1] = convert(p.phrases[phrase].rows[row].fx[fx][1]);
    for (int table = 0; table < PROJECT_MAX_TABLES; ++table) for (int row = 0; row < 16; ++row) for (int fx = 0; fx < 4; ++fx)
      if (p.tables[table].rows[row].fx[fx][0] == fxSDT) p.tables[table].rows[row].fx[fx][1] = convert(p.tables[table].rows[row].fx[fx][1]);
  }
  if (projectFileVersion < 7) {
    // SPL redefinition: SLP 1=loop / 2=ping-pong became SPL 01=reverse /
    // 02=loop / 03=ping-pong. Shift the old loop values so existing songs
    // keep their playback mode.
    for (int phrase = 0; phrase < PROJECT_MAX_PHRASES; ++phrase)
      for (int row = 0; row < 16; ++row)
        for (int fx = 0; fx < 3; ++fx)
          if (p.phrases[phrase].rows[row].fx[fx][0] == fxSLP &&
              p.phrases[phrase].rows[row].fx[fx][1] >= 1 &&
              p.phrases[phrase].rows[row].fx[fx][1] <= 2)
            p.phrases[phrase].rows[row].fx[fx][1] += 1;
    for (int table = 0; table < PROJECT_MAX_TABLES; ++table)
      for (int row = 0; row < 16; ++row)
        for (int fx = 0; fx < 4; ++fx)
          if (p.tables[table].rows[row].fx[fx][0] == fxSLP &&
              p.tables[table].rows[row].fx[fx][1] >= 1 &&
              p.tables[table].rows[row].fx[fx][1] <= 2)
            p.tables[table].rows[row].fx[fx][1] += 1;
  }
  projectFree(project);
  *project = p;
  return 0;
}

static int pathIsAbsolute(const char* path) {
  if (path[0] == '/' || path[0] == '\\') return 1;
#ifdef _WIN32
  return isalpha((unsigned char)path[0]) && path[1] == ':';
#else
  return 0;
#endif
}

static uint16_t legacyPhraseVolume(uint16_t value) {
  if (value == EMPTY_VALUE_16 || value == EMPTY_VALUE_8) return EMPTY_VALUE_16;
  return (std::min(value, uint16_t(15)) * PHRASE_VOLUME_MAX + 7) / 15;
}

static uint16_t scanPhraseVolume(char* str) {
  if (str[0] == '-' && str[1] == '-') return EMPTY_VALUE_16;
  uint8_t value;
  if (sscanf(str, "%2hhX", &value) != 1) return EMPTY_VALUE_16;
  if (projectFileVersion < 6)
    return legacyPhraseVolume(value);
  return value > PHRASE_VOLUME_MAX ? PHRASE_VOLUME_MAX : value;
}

struct CctZipEntry {
  std::string name;
  std::vector<uint8_t> data;
  uint32_t crc = 0;
  uint32_t offset = 0;
};

static uint32_t cctCrc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
  }
  return ~crc;
}

static void cctPut16(FILE* file, uint16_t value) { fputc(value & 255, file); fputc(value >> 8, file); }
static void cctPut32(FILE* file, uint32_t value) { cctPut16(file, value); cctPut16(file, value >> 16); }
static uint16_t cctGet16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t cctGet32(const uint8_t* p) { return cctGet16(p) | ((uint32_t)cctGet16(p + 2) << 16); }
static bool cctLooksLikeZip(const std::vector<uint8_t>& data) {
  return data.size() >= 4 && cctGet32(data.data()) == 0x04034b50;
}

static int cctWriteZip(FILE* file, std::vector<CctZipEntry>& entries) {
  for (CctZipEntry& entry : entries) {
    entry.offset = (uint32_t)ftell(file);
    entry.crc = cctCrc32(entry.data.data(), entry.data.size());
    cctPut32(file, 0x04034b50); cctPut16(file, 20); cctPut16(file, 0); cctPut16(file, 0);
    cctPut16(file, 0); cctPut16(file, 0); cctPut32(file, entry.crc);
    cctPut32(file, (uint32_t)entry.data.size()); cctPut32(file, (uint32_t)entry.data.size());
    cctPut16(file, (uint16_t)entry.name.size()); cctPut16(file, 0);
    fwrite(entry.name.data(), 1, entry.name.size(), file);
    fwrite(entry.data.data(), 1, entry.data.size(), file);
  }
  uint32_t centralOffset = (uint32_t)ftell(file);
  for (const CctZipEntry& entry : entries) {
    cctPut32(file, 0x02014b50); cctPut16(file, 20); cctPut16(file, 20); cctPut16(file, 0); cctPut16(file, 0);
    cctPut16(file, 0); cctPut16(file, 0); cctPut32(file, entry.crc);
    cctPut32(file, (uint32_t)entry.data.size()); cctPut32(file, (uint32_t)entry.data.size());
    cctPut16(file, (uint16_t)entry.name.size()); cctPut16(file, 0); cctPut16(file, 0); cctPut16(file, 0);
    cctPut16(file, 0); cctPut32(file, 0); cctPut32(file, entry.offset);
    fwrite(entry.name.data(), 1, entry.name.size(), file);
  }
  uint32_t centralSize = (uint32_t)ftell(file) - centralOffset;
  cctPut32(file, 0x06054b50); cctPut16(file, 0); cctPut16(file, 0);
  cctPut16(file, (uint16_t)entries.size()); cctPut16(file, (uint16_t)entries.size());
  cctPut32(file, centralSize); cctPut32(file, centralOffset); cctPut16(file, 0);
  return ferror(file) ? 1 : 0;
}

static int cctReadZip(const std::vector<uint8_t>& zip, const char* name, std::vector<uint8_t>* output) {
  if (zip.size() < 22) return 1;
  size_t start = zip.size() > 0xFFFF + 22 ? zip.size() - (0xFFFF + 22) : 0;
  size_t eocd = zip.size();
  for (size_t i = zip.size() - 22; i >= start; --i) {
    if (cctGet32(&zip[i]) == 0x06054b50) { eocd = i; break; }
    if (i == 0) break;
  }
  if (eocd == zip.size()) return 1;
  uint16_t count = cctGet16(&zip[eocd + 10]);
  uint32_t centralSize = cctGet32(&zip[eocd + 12]);
  uint32_t centralOffset = cctGet32(&zip[eocd + 16]);
  if ((uint64_t)centralOffset + centralSize > zip.size()) return 1;
  size_t p = centralOffset;
  for (uint16_t i = 0; i < count; ++i) {
    if (p + 46 > zip.size() || cctGet32(&zip[p]) != 0x02014b50) return 1;
    uint16_t nameLen = cctGet16(&zip[p + 28]), extraLen = cctGet16(&zip[p + 30]), commentLen = cctGet16(&zip[p + 32]);
    uint32_t size = cctGet32(&zip[p + 24]), offset = cctGet32(&zip[p + 42]);
    if (p + 46 + nameLen + extraLen + commentLen > zip.size()) return 1;
    if (strlen(name) == nameLen && !memcmp(&zip[p + 46], name, nameLen)) {
      if ((uint64_t)offset + 30 > zip.size() || cctGet32(&zip[offset]) != 0x04034b50) return 1;
      uint16_t localNameLen = cctGet16(&zip[offset + 26]), localExtraLen = cctGet16(&zip[offset + 28]);
      size_t dataOffset = offset + 30 + localNameLen + localExtraLen;
      if ((uint64_t)dataOffset + size > zip.size() || cctGet16(&zip[offset + 8]) != 0) return 1;
      output->assign(zip.begin() + dataOffset, zip.begin() + dataOffset + size);
      return 0;
    }
    p += 46 + nameLen + extraLen + commentLen;
  }
  return 1;
}

static int cctReadFile(const char* path, std::vector<uint8_t>* data) {
  FILE* file = fopen(path, "rb"); if (!file) return 1;
  if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 1; }
  long size = ftell(file); if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 1; }
  data->resize((size_t)size);
  int ok = fread(data->data(), 1, data->size(), file) == data->size(); fclose(file); return ok ? 0 : 1;
}

static void cctAppendSampleWav(const InstrumentSample* sample, std::vector<uint8_t>* wav,
                               uint16_t wavetableFrameSize = 0) {
  uint32_t channels = sample->channels >= 2 ? 2 : 1;
  uint32_t dataBytes = sample->frameCount * channels * 2;
  wav->resize(44 + dataBytes); uint8_t* p = wav->data();
  memcpy(p, "RIFF", 4); uint32_t riffSize = 36 + dataBytes;
  for (int i = 0; i < 4; ++i) p[4 + i] = riffSize >> (i * 8);
  memcpy(p + 8, "WAVEfmt ", 8); p[16] = 16; p[20] = 1; p[22] = channels;
  uint32_t rate = sample->sampleRate, byteRate = rate * channels * 2;
  for (int i = 0; i < 4; ++i) { p[24 + i] = rate >> (i * 8); p[28 + i] = byteRate >> (i * 8); }
  p[32] = channels * 2; p[34] = 16; memcpy(p + 36, "data", 4);
  for (int i = 0; i < 4; ++i) p[40 + i] = dataBytes >> (i * 8);
  memcpy(p + 44, sample->data, dataBytes);
  // Keep the wavetable's cycle length in the WAV itself, using the same
  // Serum metadata understood by the normal loader. PCM alone loses it.
  if (wavetableFrameSize) {
    char layout[32];
    uint32_t length = (uint32_t)snprintf(layout, sizeof(layout), "<!>%u", wavetableFrameSize);
    size_t offset = wav->size();
    wav->resize(offset + 8 + length + (length & 1), 0);
    p = wav->data();
    memcpy(p + offset, "clm ", 4);
    for (int i = 0; i < 4; ++i) p[offset + 4 + i] = length >> (i * 8);
    memcpy(p + offset + 8, layout, length);
    riffSize = (uint32_t)wav->size() - 8;
    for (int i = 0; i < 4; ++i) p[4 + i] = riffSize >> (i * 8);
  }
}

static bool cctHasSamples(const Project* project) {
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
    const Instrument* instrument = &project->instruments[i];
    const InstrumentSample* samples[2] = {NULL, NULL}; int count = 0;
    if (instrument->type == InstrumentType::Sample) samples[count++] = &instrument->chip.sample;
    else if (instrument->type == InstrumentType::SCWF || instrument->type == InstrumentType::BYOWTBL) {
      samples[count++] = &instrument->chip.scwf.oscillator[0]; samples[count++] = &instrument->chip.scwf.oscillator[1];
    }
    for (int j = 0; j < count; ++j) if (samples[j]->data && samples[j]->frameCount) return true;
  }
  return false;
}

static void projectLoadRelativeSamples(Project* project, const char* projectPath) {
  const char* slash = strrchr(projectPath, '/');
  const char* backslash = strrchr(projectPath, '\\');
  if (!slash || (backslash && backslash > slash)) slash = backslash;
  if (!slash) return;

  size_t directoryLength = (size_t)(slash - projectPath + 1);
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
    Instrument* instrument = &project->instruments[i];
    InstrumentSample* samples[2] = {NULL, NULL};
    int count = 0;
    if (instrument->type == InstrumentType::Sample) {
      samples[count++] = &instrument->chip.sample;
    } else if (instrument->type == InstrumentType::SCWF) {
      samples[count++] = &instrument->chip.scwf.oscillator[0];
      samples[count++] = &instrument->chip.scwf.oscillator[1];
    } else if (instrument->type == InstrumentType::BYOWTBL) {
      samples[count++] = &instrument->chip.byowtbl.oscillator[0];
      samples[count++] = &instrument->chip.byowtbl.oscillator[1];
    }
    for (int j = 0; j < count; ++j) {
      InstrumentSample* sample = samples[j];
      if (sample->data || !sample->path[0] || pathIsAbsolute(sample->path)) continue;
      char fullPath[1024];
      if (directoryLength + strlen(sample->path) >= sizeof(fullPath)) continue;
      memcpy(fullPath, projectPath, directoryLength);
      strcpy(fullPath + directoryLength, sample->path);
      char error[64];
      if (instrument->type == InstrumentType::BYOWTBL) {
        int index = sample == &instrument->chip.byowtbl.oscillator[1];
        srWavetableLoadWav(fullPath, sample, &instrument->chip.byowtbl.frameSize[index],
                         &instrument->chip.byowtbl.tableFrames[index], error, sizeof(error));
      } else {
        sampleLoadWav16(fullPath, sample, error, sizeof(error));
      }
    }
  }
}

int projectLoad(Project* p, const char* path) {
  projectFileError[0] = 0;
  resetPeekConsume();  // Ensure clean state

  std::vector<uint8_t> archive;
  if (cctReadFile(path, &archive) == 0 && cctLooksLikeZip(archive)) {
    std::vector<uint8_t> projectData;
    if (cctReadZip(archive, "project.cct", &projectData)) {
      snprintf(projectFileError, 40, "Invalid project archive");
      return 1;
    }
    FILE* projectFile = tmpfile();
    if (!projectFile || fwrite(projectData.data(), 1, projectData.size(), projectFile) != projectData.size()) {
      if (projectFile) fclose(projectFile);
      snprintf(projectFileError, 40, "Cannot read project archive");
      return 1;
    }
    rewind(projectFile);
    int result = projectLoadInternal(projectFile, p);
    fclose(projectFile);
    if (result) return result;
    int sampleIndex = 0;
    for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
      Instrument* instrument = &p->instruments[i];
      InstrumentSample* samples[2] = {NULL, NULL}; int count = 0;
      if (instrument->type == InstrumentType::Sample) samples[count++] = &instrument->chip.sample;
      else if (instrument->type == InstrumentType::SCWF || instrument->type == InstrumentType::BYOWTBL) {
        samples[count++] = &instrument->chip.scwf.oscillator[0]; samples[count++] = &instrument->chip.scwf.oscillator[1];
      }
      for (int j = 0; j < count; ++j, ++sampleIndex) {
        char name[32]; snprintf(name, sizeof(name), "samples/%03d.wav", sampleIndex);
        std::vector<uint8_t> wav;
        if (cctReadZip(archive, name, &wav)) continue;
        FILE* sampleFile = tmpfile();
        if (!sampleFile || fwrite(wav.data(), 1, wav.size(), sampleFile) != wav.size()) {
          if (sampleFile) fclose(sampleFile);
          continue;
        }
        rewind(sampleFile);
        char error[64];
        if (instrument->type == InstrumentType::BYOWTBL) {
          srWavetableLoadWavFile(sampleFile, samples[j]->path, samples[j],
                                &instrument->chip.byowtbl.frameSize[j],
                                &instrument->chip.byowtbl.tableFrames[j], error, sizeof(error));
        } else {
          sampleLoadWav16File(sampleFile, samples[j]->path, samples[j], error, sizeof(error));
        }
        fclose(sampleFile);
      }
    }
    projectLoadRelativeSamples(p, path);
    return 0;
  }

  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    snprintf(projectFileError, 40, "Can't open file");
    return 1;
  }

  int result = projectLoadInternal(file, p);
  fclose(file);
  if (!result) projectLoadRelativeSamples(p, path);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Save functions

static int projectSavePitchTable(FILE* file, Project* project) {
  fprintf(file, "\n## Pitch table\n\n");
  fprintf(file, "- Title: %s\n\n```\n", project->pitchTable.name);

  for (int c = 0; c < project->pitchTable.length; c++) {
    fprintf(file, "%s %d\n", project->pitchTable.noteNames[c], project->pitchTable.values[c]);
  }

  fprintf(file, "```\n");

  return 0;
}

static int projectSaveSong(FILE* file, Project* project) {

  fprintf(file, "\n## Song\n\n```\n");

  // Find the last row with values
  int songLength = PROJECT_MAX_LENGTH;
  for (songLength = PROJECT_MAX_LENGTH - 1; songLength >= 0; songLength--) {
    int isEmpty = 1;
    for (int c = 0; c < project->tracksCount; c++) {
      if (project->song[songLength][c] != EMPTY_VALUE_16) {
        isEmpty = 0;
        break;
      }
    }
    if (!isEmpty) {
      break;
    }
  }
  songLength++;

  for (int c = 0; c < songLength; c++) {
    for (int d = 0; d < project->tracksCount; d++) {
      int chain = project->song[c][d];
      int isHighlighted = project->songHighlight[c][d];
      if (chain == EMPTY_VALUE_16) {
        fprintf(file, "--");
      } else {
        fprintf(file, "%s", byteToHex(chain));
      }
      // Add asterisk if highlighted, otherwise space (except after last column)
      if (isHighlighted) {
        fprintf(file, "*");
      } else if (d < project->tracksCount - 1) {
        fprintf(file, " ");
      }
    }
    fprintf(file, "\n");
  }

  fprintf(file, "```\n");

  return 0;
}

static int projectSaveChains(FILE* file, Project* project) {
  fprintf(file, "\n## Chains\n");

  for (int c = 0; c < PROJECT_MAX_CHAINS; c++) {
    if (!chainIsEmpty(project, c)) {
      fprintf(file, "\n### Chain %X\n\n```\n", c);
      for (int d = 0; d < 16; d++) {
        int phrase = project->chains[c].rows[d].phrase;
        if (phrase == EMPTY_VALUE_16) {
          fprintf(file, "--- %s\n", byteToHex(project->chains[c].rows[d].transpose));
        } else {
          fprintf(file, "%03X %s\n", project->chains[c].rows[d].phrase, byteToHex(project->chains[c].rows[d].transpose));
        }
      }
      fprintf(file, "```\n");
    }
  }

  return 0;
}

static int projectSaveGrooves(FILE* file, Project* project) {
  fprintf(file, "\n## Grooves\n");

  for (int c = 0; c < PROJECT_MAX_GROOVES; c++) {
    if (!grooveIsEmpty(project, c)) {
      fprintf(file, "\n### Groove %X\n\n```\n", c);
      for (int d = 0; d < 16; d++) {
        fprintf(file, "%s\n", byteToHexOrEmpty(project->grooves[c].speed[d]));
      }
      fprintf(file, "```\n");
    }
  }

  return 0;
}

// File spelling of an FX name. SPL (sample playback) keeps its legacy
// "SLP" spelling: the AY2 Pulse Low Level FX already owns "SPL" in the
// flat FX namespace scanFX resolves, so writing "SPL" for the sample FX
// would hijack loads of both old and new files.
static const char* fxSaveName(uint8_t fx) {
  return fx == fxSLP ? "SLP" : fxNames[fx].name;
}

static int projectSavePhrases(FILE* file, Project* project) {

  fprintf(file, "\n## Phrases\n\n");

  for (int c = 0; c < PROJECT_MAX_PHRASES; c++) {
    if (!phraseIsEmpty(project, c)) {
      fprintf(file, "### Phrase %X\n\n```\n", c);
      for (int d = 0; d < 16; d++) {
        fprintf(file, "%s %s %s %s %s %s %s %s %s\n",
          noteName(project, project->phrases[c].rows[d].note),
          byteToHexOrEmpty(project->phrases[c].rows[d].instrument),
          volumeToHexOrEmpty(project->phrases[c].rows[d].volume),
          fxSaveName(project->phrases[c].rows[d].fx[0][0]),
          byteToHex(project->phrases[c].rows[d].fx[0][1]),
          fxSaveName(project->phrases[c].rows[d].fx[1][0]),
          byteToHex(project->phrases[c].rows[d].fx[1][1]),
          fxSaveName(project->phrases[c].rows[d].fx[2][0]),
          byteToHex(project->phrases[c].rows[d].fx[2][1])
        );
      }
      fprintf(file, "```\n");
    }
  }

  return 0;
}

int saveTable(FILE* file, int idx, Table* table) {

  fprintf(file, "\n### Table %X (Retrig: %s)\n\n```\n", idx, tableRetriggerModeName(table->retriggerMode));
  for (int d = 0; d < 16; d++) {
    fprintf(file, "%c %s %s %s %s %s %s %s %s %s %s\n",
      table->rows[d].pitchFlag ? '=' : '~',
      byteToHex(table->rows[d].pitchOffset),
      byteToHexOrEmpty(table->rows[d].volume),
      fxSaveName(table->rows[d].fx[0][0]), byteToHex(table->rows[d].fx[0][1]),
      fxSaveName(table->rows[d].fx[1][0]), byteToHex(table->rows[d].fx[1][1]),
      fxSaveName(table->rows[d].fx[2][0]), byteToHex(table->rows[d].fx[2][1]),
      fxSaveName(table->rows[d].fx[3][0]), byteToHex(table->rows[d].fx[3][1]));
  }
  fprintf(file, "```\n");
  return 0;
}

static int projectSaveInstruments(FILE* file, Project* project) {
  fprintf(file, "\n## Instruments\n");
  for (int c = 0; c < PROJECT_MAX_INSTRUMENTS; c++) {
    if (!instrumentIsEmpty(project, c)) {
      instrumentSaveData(file, c, &project->instruments[c]);
    }
  }
  return 0;
}

static int projectSaveTables(FILE* file, Project* project) {
  fprintf(file, "\n## Tables\n");
  for (int c = 0; c < PROJECT_MAX_TABLES; c++) {
    if (!tableIsEmpty(project, c)) {
      saveTable(file, c, &project->tables[c]);
    }
  }
  return 0;
}

static int projectSaveAYWavetables(FILE* file, Project* project) {
  // Count non-empty wavetables
  int count = 0;
  for (int i = 0; i < 256; i++) {
    if (!wavetableIsEmpty(project, i)) count++;
  }

  // Only write section if there are non-empty wavetables
  if (count == 0) return 0;

  fprintf(file, "\n## AY Wavetables\n\n");

  for (int i = 0; i < 256; i++) {
    if (!wavetableIsEmpty(project, i)) {
      // Write: "XX 0123456789ABCDEF0123456789ABCDEF"
      fprintf(file, "%02X ", i);
      for (int j = 0; j < 32; j++) {
        fprintf(file, "%X", project->ayWavetables[i][j] & 0x0F);
      }
      fprintf(file, "\n");
    }
  }

  return 0;
}

static int projectSaveInternal(FILE* file, Project* project) {
  bool nativeChips = false;
  for (const auto& instrument : project->instruments) nativeChips |= (instrument.type==InstrumentType::SID || instrument.type==InstrumentType::DX7 || isOPLL(instrument.type) || (isOPL(instrument.type) || isFourOp(instrument.type)) || isSimpleChip(instrument.type));
  for (const auto& phrase : project->phrases) for (const auto& row : phrase.rows) for (const auto& fx : row.fx) nativeChips |= fx[0] >= fxFBR && fx[0] < fxTotalCount;
  for (const auto& table : project->tables) for (const auto& row : table.rows) for (const auto& fx : row.fx) nativeChips |= fx[0] >= fxFBR && fx[0] < fxTotalCount;
  bool sourcePrograms=false;
  for(const auto& i:project->instruments)sourcePrograms |= i.type==InstrumentType::SID?bool(i.chip.sid.program.format):isSimpleChip(i.type)?bool(i.chip.simpleChip.program.format):false;
  // Native formats 6-8 predate upstream's expanded phrase volume. Format 9
  // distinguishes new 00-7F songs while retaining their native patches and FX.
  // Plain projects stay at 7.0: this fork's 7.0 carries the redefined SLP
  // playback values (0=fwd/1=rev/2=loop/3=ping-pong) and the 6-field Scale
  // line, so writing 6.0 would re-trigger the <7 SLP value migration on
  // reload and corrupt loop modes. Upstream never wrote 7.0 files, so the
  // number stays unambiguous. Native-chip projects use upstream's 9/10.
  fprintf(file, "# ChooChooTracker Module %d.0\n\n", sourcePrograms ? 10 : nativeChips ? 9 : 7);

  fprintf(file, "- Title: %s\n", project->title);
  fprintf(file, "- Author: %s\n", project->author);

  fprintf(file, "- Frame rate: %f\n", project->tickRate);
  fprintf(file, "- Chips count: %d\n", project->chipsCount);
  fprintf(file, "- Track inserts: 1,8,2\n");
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) for (int slot = 0; slot < 2; ++slot) {
    const auto& c = project->trackInserts[t][slot];
    fprintf(file, "- Insert: %d,%d,%u,%u",t,slot,c.module,c.bypass);
    for (int i = 0; i < 8; ++i) fprintf(file, ",%u", c.values[i]);
    fprintf(file, "\n");
  }
  fprintf(file, "- Track volumes: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n",
    project->trackVolume[0], project->trackVolume[1], project->trackVolume[2], project->trackVolume[3],
    project->trackVolume[4], project->trackVolume[5], project->trackVolume[6], project->trackVolume[7]);
  fprintf(file, "- Track pans: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n",
    project->trackPan[0], project->trackPan[1], project->trackPan[2], project->trackPan[3],
    project->trackPan[4], project->trackPan[5], project->trackPan[6], project->trackPan[7]);
  fprintf(file, "- Reverb sends: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n",
    project->trackReverbSend[0], project->trackReverbSend[1], project->trackReverbSend[2], project->trackReverbSend[3],
    project->trackReverbSend[4], project->trackReverbSend[5], project->trackReverbSend[6], project->trackReverbSend[7]);
  fprintf(file, "- Delay sends: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n",
    project->trackDelaySend[0], project->trackDelaySend[1], project->trackDelaySend[2], project->trackDelaySend[3],
    project->trackDelaySend[4], project->trackDelaySend[5], project->trackDelaySend[6], project->trackDelaySend[7]);
  fprintf(file, "- Track tilts: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n",
    project->trackTilt[0], project->trackTilt[1], project->trackTilt[2], project->trackTilt[3],
    project->trackTilt[4], project->trackTilt[5], project->trackTilt[6], project->trackTilt[7]);
  fprintf(file, "- Reverb: %hhu,%hhu,%hhu,%hu\n", project->reverbReturn, project->reverbTime,
    project->reverbDamping, project->reverbFilterCutoffHz);
  fprintf(file, "- Delay: %hhu,%hhu,%hhu,%hhu,%hu\n", project->delayReturn, project->delayReverbSend,
    project->delayTicks, project->delayFeedback, project->delayFilterCutoffHz);
  fprintf(file, "- Tilt pivot: %hu\n", project->tiltPivotHz);
  // Optional field (Phase 4): only written when set, so older versions of
  // the format stay byte-identical for projects without the preference.
  // Must stay before the MIDI CC mappings block: the loader reads it right
  // after the tilt pivot line (this fork's layout, kept from before the
  // upstream merge).
  if (project->sampleSaveChoice) {
    fprintf(file, "- Sample save choice: %hhu\n", project->sampleSaveChoice);
  }
  fprintf(file, "- MIDI CC mappings: %d\n", PROJECT_MAX_MIDI_CC_MAPPINGS);
  for (int i = 0; i < PROJECT_MAX_MIDI_CC_MAPPINGS; ++i) {
    const MidiCCMapping& m = project->midiCCMappings[i];
    fprintf(file, "- MIDI CC: %d,%d,%d,%d,%d,%d\n", i, m.enabled, m.channel, m.cc,
            m.instrument, m.destination);
  }
  fprintf(file, "- Linear pitch: %d\n", project->linearPitch);
  fprintf(file, "- Signed track speed: %d\n", project->signedTrackSpeed);
  fprintf(file, "- Perceptual effects: %d\n", project->perceptualEffects);
  fprintf(file, "- Scale: %d,%d,%d,%u,%u,%u\n", project->scaleApply, project->scaleMode, project->scaleRoot,
          (unsigned)project->scalePreset, project->scaleCustomMask, project->scaleTracksMask);
  fprintf(file, "- Chip type: %s\n", chipNames[static_cast<int>(project->chipType)]);

  switch (project->chipType) {
  case ChipType::AY:
    fprintf(file, "- *AY8910* Clock: %d\n", project->chipSetup.ay.clock);
    fprintf(file, "- *AY8910* AY/YM: %d\n", project->chipSetup.ay.isYM);
    switch (project->chipSetup.ay.stereoMode) {
    case StereoModeAY::ABC:
      fprintf(file, "- *AY8910* Stereo: ABC\n");
      break;
    case StereoModeAY::ACB:
      fprintf(file, "- *AY8910* Stereo: ACB\n");
      break;
    case StereoModeAY::BAC:
      fprintf(file, "- *AY8910* Stereo: BAC\n");
      break;
    }
    fprintf(file, "- *AY8910* Stereo separation: %d\n", project->chipSetup.ay.stereoSeparation);
    fprintf(file, "- *AY8910* PWM range: %d\n", project->chipSetup.ay.pwmFullRange);
    break;
  default:
    break;
  }

  projectSavePitchTable(file, project);
  projectSaveSong(file, project);
  projectSaveChains(file, project);
  projectSaveGrooves(file, project);
  projectSavePhrases(file, project);
  projectSaveInstruments(file, project);
  projectSaveTables(file, project);
  projectSaveAYWavetables(file, project);
  fprintf(file, "EOF\n");
  return 0;
}

static int closeProjectOutput(FILE* file, int result) {
  if (!file) return 1;
  if (result == 0 && fflush(file) != 0) result = 1;
  if (ferror(file)) result = 1;
  if (fclose(file) != 0) result = 1;
  return result;
}

static int projectSaveDirect(Project* p, const char* path) {
  if (cctHasSamples(p)) {
    FILE* projectFile = tmpfile();
    if (!projectFile) {
      snprintf(projectFileError, 40, "Can't create temporary project");
      return 1;
    }

    int projectResult = projectSaveInternal(projectFile, p);
    if (projectResult != 0 || fflush(projectFile) != 0 || ferror(projectFile)) {
      fclose(projectFile);
      snprintf(projectFileError, 40, "Can't serialize project");
      return 1;
    }

    long projectSize = ftell(projectFile);
    if (projectSize < 0 || fseek(projectFile, 0, SEEK_SET) != 0) {
      fclose(projectFile);
      snprintf(projectFileError, 40, "Can't prepare project archive");
      return 1;
    }

    CctZipEntry projectEntry;
    projectEntry.name = "project.cct";
    projectEntry.data.resize((size_t)projectSize);
    if (fread(projectEntry.data.data(), 1, projectEntry.data.size(), projectFile) != projectEntry.data.size()) {
      fclose(projectFile);
      snprintf(projectFileError, 40, "Can't read serialized project");
      return 1;
    }
    fclose(projectFile);

    std::vector<CctZipEntry> entries;
    entries.push_back(std::move(projectEntry));
    int sampleIndex = 0;
    for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
      Instrument* instrument = &p->instruments[i];
      InstrumentSample* samples[2] = {NULL, NULL};
      int count = 0;
      if (instrument->type == InstrumentType::Sample) {
        samples[count++] = &instrument->chip.sample;
      } else if (instrument->type == InstrumentType::SCWF || instrument->type == InstrumentType::BYOWTBL) {
        samples[count++] = &instrument->chip.scwf.oscillator[0];
        samples[count++] = &instrument->chip.scwf.oscillator[1];
      }

      for (int j = 0; j < count; ++j, ++sampleIndex) {
        if (!samples[j]->data || !samples[j]->frameCount) continue;
        CctZipEntry sample;
        char name[32];
        snprintf(name, sizeof(name), "samples/%03d.wav", sampleIndex);
        sample.name = name;
        cctAppendSampleWav(samples[j], &sample.data,
                           instrument->type == InstrumentType::BYOWTBL ? instrument->chip.byowtbl.frameSize[j] : 0);
        entries.push_back(std::move(sample));
      }
    }

    FILE* file = fopen(path, "wb");
    if (!file) {
      snprintf(projectFileError, 40, "Can't open save file");
      return 1;
    }
    int result = cctWriteZip(file, entries);
    result = closeProjectOutput(file, result);
    if (result != 0 && projectFileError[0] == 0) {
      snprintf(projectFileError, 40, "Can't write save file");
    }
    return result;
  }

  FILE* file = fopen(path, "wb");
  if (!file) {
    snprintf(projectFileError, 40, "Can't open save file");
    return 1;
  }

  int result = projectSaveInternal(file, p);
  result = closeProjectOutput(file, result);
  if (result != 0 && projectFileError[0] == 0) {
    snprintf(projectFileError, 40, "Can't write save file");
  }
  return result;
}

#ifndef WEB_BUILD
static int replaceProjectFile(const char* tempPath, const char* path) {
#ifdef _WIN32
  return MoveFileExA(tempPath, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : 1;
#else
  return rename(tempPath, path) == 0 ? 0 : 1;
#endif
}
#endif

int projectSave(Project* p, const char* path) {
  projectFileError[0] = 0;

#ifdef WEB_BUILD
  // The browser filesystem does not provide the rename semantics used by the
  // native transactional path. Keep the existing direct-save behavior there.
  return projectSaveDirect(p, path);
#else
  // Write beside the destination first. A failed serialization or close never
  // truncates the previous valid project/autosave.
  std::string tempPath = std::string(path) + ".tmp";
  int result = projectSaveDirect(p, tempPath.c_str());
  if (result != 0) {
    remove(tempPath.c_str());
    return result;
  }

  if (replaceProjectFile(tempPath.c_str(), path) != 0) {
    remove(tempPath.c_str());
    snprintf(projectFileError, 40, "Can't replace save file");
    return 1;
  }

  return 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Instrument file I/O

int instrumentSave(Project* project, const char* path, int instrumentIdx) {
  projectFileError[0] = 0;

  FILE* file = fopen(path, "wb");
  if (file == NULL) {
    snprintf(projectFileError, 40, "Can't open file");
    return 1;
  }

  bool nativeFormat = (project->instruments[instrumentIdx].type==InstrumentType::SID || project->instruments[instrumentIdx].type==InstrumentType::DX7 || isOPLL(project->instruments[instrumentIdx].type) || (isOPL(project->instruments[instrumentIdx].type) || isFourOp(project->instruments[instrumentIdx].type)) || isSimpleChip(project->instruments[instrumentIdx].type));
  for (const auto& row : project->tables[instrumentIdx].rows) for (const auto& fx : row.fx) nativeFormat |= fx[0] >= fxFBR && fx[0] < fxTotalCount;
  bool absoluteLevels=false;
  for(const auto& row:project->tables[instrumentIdx].rows)for(const auto& fx:row.fx)absoluteLevels |= fx[0]>=fxOL1&&fx[0]<=fxFBK;
  const auto& inst=project->instruments[instrumentIdx];
  bool sourceProgram=inst.type==InstrumentType::SID?bool(inst.chip.sid.program.format):isSimpleChip(inst.type)?bool(inst.chip.simpleChip.program.format):false;
  fprintf(file, "# ChipNomad Instrument %d.0\n\n", sourceProgram ? 9 : absoluteLevels ? 8 : nativeFormat ? 7 : 5);
  instrumentSaveData(file, 0, &project->instruments[instrumentIdx]);
  saveTable(file, 0, &project->tables[instrumentIdx]);

  fclose(file);
  return 0;
}

static int instrumentLoadInternal(FILE* file, Project* project, int instrumentIdx) {
  char* line = peekLine(file);
  if (line == NULL) return 1;
  if (strncmp(line, "# ChipNomad Instrument", 22)) {
    snprintf(projectFileError, 40, "Incorrect instrument format");
    return 1;
  }

  // Detect version
  if (strlen(line) > 22) {
    if (strncmp(line + 22, " 9.0", 4) == 0) {
      projectFileVersion = 9;
    } else if (strncmp(line + 22, " 8.0", 4) == 0) {
      projectFileVersion = 8;
    } else if (strncmp(line + 22, " 7.0", 4) == 0) {
      projectFileVersion = 7;
    } else if (strncmp(line + 22, " 6.0", 4) == 0) {
      projectFileVersion = 6;
    } else if (strncmp(line + 22, " 5.0", 4) == 0) {
      projectFileVersion = 5;
    } else if (strncmp(line + 22, " 4.0", 4) == 0) {
      projectFileVersion = 4;
    } else if (strncmp(line + 22, " 3.0", 4) == 0) {
      projectFileVersion = 3;
    } else if (strncmp(line + 22, " 2.0", 4) == 0) {
      projectFileVersion = 2;
    } else if (strncmp(line + 22, " 1.0", 4) == 0) {
      projectFileVersion = 1;
    } else {
      snprintf(projectFileError, 40, "Incorrect instrument version");
      return 1;
    }
  } else {
    // No version specified = 1.0 (legacy)
    projectFileVersion = 1;
  }
  consumeLine(file);

  snprintf(projectFileError, 40, "Invalid instrument data");
  line = peekLine(file);
  if (line == NULL) return 1;
  if (strncmp(line, "### Instrument", 14)) return 1;
  consumeLine(file);

  if (instrumentLoadData(file, &project->instruments[instrumentIdx], project)) return 1;
  // instrumentLoadData leaves the next section header in peek buffer
  snprintf(projectFileError, 40, "Invalid table data");
  line = peekLine(file);
  if (line == NULL) {
    snprintf(projectFileError, 40, "Missing table section");
    return 1;
  }
  if (strncmp(line, "### Table", 9)) {
    snprintf(projectFileError, 40, "Expected table, got: %.20s", line);
    return 1;
  }
  project->tables[instrumentIdx].retriggerMode = tableRetriggerModeFromHeader(line);
  consumeLine(file);
  if (loadTable(file, &project->tables[instrumentIdx], project)) return 1;

  return 0;
}

static int instrumentLoadStream(Project* project, FILE* file, int instrumentIdx) {
  projectFileError[0] = 0;
  resetPeekConsume();  // Ensure clean state

  if (!file) return 1;
  int result;
  const char* header = peekLine(file);
  if (header && (strncmp(header, "# ChipNomad Instrument 6.0", 25) == 0 || strncmp(header, "# ChipNomad Instrument 7.0", 25) == 0 || strncmp(header, "# ChipNomad Instrument 8.0",25)==0 || strncmp(header,"# ChipNomad Instrument 9.0",25)==0)) {
    auto temporary = std::make_unique<Project>();
    projectInit(temporary.get());
    result = instrumentLoadInternal(file, temporary.get(), instrumentIdx);
    if (!result && projectFileVersion==6 && !(temporary->instruments[instrumentIdx].type==InstrumentType::SID || temporary->instruments[instrumentIdx].type==InstrumentType::DX7 || isOPLL(temporary->instruments[instrumentIdx].type) || (isOPL(temporary->instruments[instrumentIdx].type) || isFourOp(temporary->instruments[instrumentIdx].type)) || isSimpleChip(temporary->instruments[instrumentIdx].type))) result = 1;
    if (!result) {
      instrumentClear(&project->instruments[instrumentIdx]);
      project->instruments[instrumentIdx] = temporary->instruments[instrumentIdx];
      project->tables[instrumentIdx] = temporary->tables[instrumentIdx];
      temporary->instruments[instrumentIdx] = {}; // Ownership moved, including sample buffers.
    }
    projectFree(temporary.get());
  } else result = instrumentLoadInternal(file, project, instrumentIdx);
  return result;
}

int instrumentLoad(Project* project, const char* path, int instrumentIdx) {
  FILE* file = fopen(path, "rb");
  if (!file) { snprintf(projectFileError, 40, "Can't open file"); return 1; }
  int result = instrumentLoadStream(project, file, instrumentIdx);
  fclose(file); return result;
}

int instrumentLoadMemory(Project* project, const uint8_t* bytes, size_t size, int instrumentIdx) {
  if (!project || !bytes || !size || size > 1024 * 1024 || instrumentIdx < 0 || instrumentIdx >= PROJECT_MAX_INSTRUMENTS) return 1;
  FILE* file = tmpfile();
  if (!file) { snprintf(projectFileError, 40, "Cannot read preset buffer"); return 1; }
  bool ok = fwrite(bytes, 1, size, file) == size && !fseek(file, 0, SEEK_SET);
  int result = ok ? instrumentLoadStream(project, file, instrumentIdx) : 1;
  fclose(file); return result;
}
