CXX = c++
CFLAGS = -std=c++17 -O2 -DTEST -I/opt/homebrew/include -I/opt/homebrew/include/freetype2
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lfreetype -lm

# Directories
BUILD_DIR = build

# Include paths
INCLUDES = -Isrc -I../chipnomad_lib -I../chipnomad_lib/chips -I../chipnomad_lib/external/ayumi

# Find source files
SOURCES = $(wildcard src/*.cpp) \
          $(wildcard ../chipnomad_lib/*.cpp) \
          $(wildcard ../chipnomad_lib/chips/*.cpp) \
          $(wildcard ../chipnomad_lib/synth/*.cpp) \
          $(wildcard ../chipnomad_lib/external/ayumi/*.cpp) \
          $(wildcard ../chipnomad_lib/external/mutable/braids/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/stmlib/utils/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/stmlib/dsp/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/chords/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/engine/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/engine2/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/fm/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/physical_modelling/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/speech/*.cc)

# Target
TARGET = choochooplayer

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
