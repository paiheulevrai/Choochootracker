#pragma once
#include "project_instruments.h"
#include <cstdio>
#include <string>
#include <vector>
bool validDX7(const InstrumentDX7& patch);
void initDX7Patch(InstrumentDX7* patch);
int loadDX7Data(FILE* file, Instrument* instrument);
int saveDX7Data(FILE* file, const Instrument* instrument);
// Validated Yamaha DX7 single-voice (155-byte) or 32-voice (4096-byte) bulk.
// All messages are validated before output changes; no MIDI transmission.
bool importDX7SysEx(const uint8_t* bytes, size_t size,
                    std::vector<InstrumentDX7>& output, std::string& error);
