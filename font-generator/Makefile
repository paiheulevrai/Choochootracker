CC = gcc
# pkg-config finds FreeType on Linux and Homebrew (macOS: it may need
# PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig). Falls back to Homebrew paths.
FT_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null || echo -I/opt/homebrew/include/freetype2)
FT_LIBS := $(shell pkg-config --libs freetype2 2>/dev/null || echo -lfreetype -L/opt/homebrew/lib)
CFLAGS = -Wall -Wextra -std=c99 $(FT_CFLAGS)
LIBS = $(FT_LIBS)

TARGET = generate
SOURCE = generate.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

clean:
	rm -f $(TARGET) font*.c font_preview.png

test: $(TARGET)
	./$(TARGET) ../font-generator/MxPlus_IBM_VGA_9x16.ttf

.PHONY: all clean test