CXX = c++
CFLAGS = -std=c++17 -O2 -DTEST -I/opt/homebrew/include -I/opt/homebrew/include/freetype2
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lfreetype -lm

# Windows (MSYS2 UCRT64) overrides.  Use: make windows
WIN_DIR = build/windows
WIN_OBJ_DIR = build/windows/obj
WIN_CXX = g++
WIN_SDL_CFLAGS = $(filter-out -Dmain=SDL_main,$(shell sdl2-config --cflags))
WIN_SDL_LIBS = $(shell sdl2-config --libs)
WIN_CFLAGS = -std=c++17 -O2 -DTEST -I/ucrt64/include/freetype2 $(WIN_SDL_CFLAGS) -D__USE_MINGW_ANSI_STDIO=1
WIN_LDFLAGS = $(WIN_SDL_LIBS) -lfreetype -lm -static-libgcc -static-libstdc++

# Directories
BUILD_DIR = build

# Include paths
INCLUDES = -Isrc -I../chipnomad_lib -I../chipnomad_lib/chips -I../chipnomad_lib/external/ayumi -I../chipnomad_lib/external/mutable -I../chipnomad_lib/external/open303/Source/DSPCode

# Find source files
SOURCES = $(wildcard src/*.cpp) \
          $(wildcard ../chipnomad_lib/*.cpp) \
          $(wildcard ../chipnomad_lib/chips/*.cpp) \
          $(wildcard ../chipnomad_lib/synth/*.cpp) \
          $(wildcard ../chipnomad_lib/external/ayumi/*.cpp) \
          $(wildcard ../chipnomad_lib/external/open303/Source/DSPCode/*.cpp) \
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
          $(wildcard ../chipnomad_lib/external/mutable/plaits/dsp/speech/*.cc) \
          ../chipnomad_lib/external/mutable/plaits_alt/resources.cc \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/chords/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/engine/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/engine2/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/fm/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/physical_modelling/*.cc) \
          $(wildcard ../chipnomad_lib/external/mutable/plaits_alt/dsp/speech/*.cc)

# Target
TARGET = choochooplayer

.PHONY: all clean windows

all: $(TARGET)

windows: $(WIN_DIR)/choochooplayer.exe
	@mkdir -p $(WIN_DIR)/fonts/Fragment_Mono
	ldd $(WIN_DIR)/choochooplayer.exe | sed -n 's/.*=> \(\/ucrt64\/bin\/[^ ]*\).*/\1/p' | xargs -r -I{} cp {} $(WIN_DIR)/
	cp player_settings.txt $(WIN_DIR)/
	cp fonts/Fragment_Mono/FragmentMono-Regular.ttf $(WIN_DIR)/fonts/Fragment_Mono/
	cp ../tracker/packaging/common/projects/alf\ dance.cct $(WIN_DIR)/
	cp -r ../tracker/packaging/common/samples ../tracker/packaging/common/SR_wavetables $(WIN_DIR)/
	sed -i 's|C:\\Users\\surga\\Desktop\\mobilegroove\\tracker\\build\\windows\\SR_wavetables\\WaveEdit\\BASIC_TH.WAV|SR_wavetables/WaveEdit/BASIC_TH.WAV|' "$(WIN_DIR)/alf dance.cct"
	sed -i 's|C:\\Users\\surga\\Desktop\\mobilegroove\\tracker\\build\\windows\\SR_wavetables\\WaveEdit\\PLESANT_.WAV|SR_wavetables/WaveEdit/PLESANT_.WAV|' "$(WIN_DIR)/alf dance.cct"
	cp launch-alf-dance.bat $(WIN_DIR)/

win_object = $(WIN_OBJ_DIR)/$(subst ../,up/,$(basename $(1))).o
WIN_OBJECTS = $(foreach source,$(SOURCES),$(call win_object,$(source)))

define compile_win_source
$(call win_object,$(1)): $(1) Makefile
	@mkdir -p $$(dir $$@)
	$$(WIN_CXX) $$(WIN_CFLAGS) $$(INCLUDES) -c $$< -o $$@
endef
$(foreach source,$(SOURCES),$(eval $(call compile_win_source,$(source))))

$(WIN_DIR)/choochooplayer.exe: $(WIN_OBJECTS)
	@mkdir -p $(WIN_DIR)
	$(WIN_CXX) $(WIN_CFLAGS) $(WIN_OBJECTS) -o $@ $(WIN_LDFLAGS)

$(TARGET): $(SOURCES)
	$(CXX) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
