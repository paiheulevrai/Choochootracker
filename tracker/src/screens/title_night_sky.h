#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Seeded pixel-art galaxy. One instance lives only while the Night title is open.
// A closed four-minute orbit avoids jumps; the nebula updates at 10 Hz while
// input frames use a 60 Hz clock, independent of the platform's draw cadence.
class TitleNightSky {
public:
  static constexpr int width = 256, height = 224;
  uint32_t pixels[width * height]; // Opaque 0xAARRGGBB, for gfxImageUpdateARGB.

  TitleNightSky() {
    for (auto& value : grid) value = random();
    for (int i = 0; i < 1100; ++i) {
      auto& s = stars[i];
      s.x = random() * 512 - 128;
      s.y = i < 220 ? random() * 176 : 126 - s.x * .35 +
        (random() + random() + random() - 1.5) * 33;
      s.depth = i % 3;
      s.light = random(); s.phase = random() * tau;
      s.pulse = 2 + (int)(random() * 5);
    }
  }

  bool render(uint64_t frame, int rows = height) {
    // Finish the current image before starting a newer phase. Callers upload
    // only complete images, so spreading rows across draws cannot expose seams.
    const int tick = nextRow ? pending : (frame / 6) % 2400;
    if (tick == last) return false;
    pending = tick;
    const double angle = tick / 2400.0 * tau;
    const double dx[] = {6 * sin(angle), 11 * sin(angle + .35), 17 * sin(angle + .7)};
    const double dy[] = {3 * cos(angle), 5 * cos(angle + .35), 7 * cos(angle + .7)};
    const double flowX = .35 * sin(angle * 2), flowY = .28 * cos(angle * 2);
    static const int palette[][3] = {{6,10,23},{9,15,33},{13,22,47},{19,30,61},
      {28,40,78},{41,51,93},{57,62,106},{77,76,121},{100,94,139},{127,118,159},{160,151,181}};
    static const int bayer[] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    const int endRow = std::min(height, nextRow + std::max(1, rows));
    for (int y = nextRow; y < endRow; ++y) for (int x = 0; x < width; ++x) {
      const double u = x + dx[0], v = y + dy[0], along = u * .94 - v * .34;
      const double broad = cloud(u * .026 + 12, v * .033 + 27);
      const double across = v - (126 - u * .35) - (broad - .5) * 17;
      const double envelope = exp(-across * across / (2 * 29 * 29));
      const double gas = cloud(along * .048 + dx[1] * .028 + 41, across * .083 + dy[1] * .036 + 19);
      const double filaments = cloud(along * .092 + flowX + 8, across * .19 + flowY + 36);
      const double spine = across - 4 - (noise(along * .035 + flowX, 51 + flowY) - .5) * 13;
      const double lane = exp(-spine * spine / 36) * (.35 + .65 * gas);
      const double core = exp(-across * across / 210), grain = grid[y * 256 + x] - .5;
      double light = .18 + broad * .35 + envelope * (.5 + gas * 2.9 + filaments * 1.35)
        + core * (.2 + gas * 2.4) - lane * 2.3 + grain * envelope * .9;
      light *= 1 - std::max(0, y - 135) / 180.0;
      const int index = std::clamp((int)floor(light + (bayer[(y & 3) * 4 + (x & 3)] + .5) / 16), 0, 10);
      // Lavender belongs only to scattered luminous wisps, never a global tint.
      const double highlight = smooth(clamp((gas - .47) * 6)) * smooth(clamp((filaments - .40) * 5))
        * clamp((light - 2.4) * 1.2) * envelope * (1 - lane);
      const auto& c = palette[index];
      pixels[y * width + x] = pack(c[0] + rounded(highlight * 30),
        c[1] + rounded(highlight * 6), c[2] + rounded(highlight * 24));
    }
    nextRow = endRow;
    if (nextRow < height) return false;
    nextRow = 0;
    last = tick;
    for (const auto& s : stars) {
      const int x = rounded(s.x + dx[s.depth]), y = rounded(s.y + dy[s.depth]);
      const double strength = (.09 + pow(s.light, 7) * .8) * (.84 + .16 * sin(angle * s.pulse + s.phase));
      starPixel(x, y, 135, 152, 192, strength);
      if (s.light > .987) {
        starPixel(x + 1, y, 76, 88, 128, strength * .30);
        starPixel(x - 1, y, 76, 88, 128, strength * .30);
        starPixel(x, y + 1, 76, 88, 128, strength * .30);
        starPixel(x, y - 1, 76, 88, 128, strength * .30);
      }
    }
    return true;
  }

private:
  static constexpr double tau = 6.283185307179586;
  struct Star { double x, y, light, phase; int depth, pulse; } stars[1100];
  float grid[65536];
  uint32_t seed = 0x43484f4f;
  int last = -1, pending = -1, nextRow = 0;
  double random() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed / 4294967296.0; }
  static double clamp(double t) { return std::clamp(t, 0.0, 1.0); }
  static double smooth(double t) { return t * t * (3 - 2 * t); }
  static double lerp(double a, double b, double t) { return a + (b - a) * t; }
  static int rounded(double v) { return (int)floor(v + .5); }
  static uint32_t pack(int r, int g, int b) { return 0xff000000u | (r << 16) | (g << 8) | b; }
  double noise(double x, double y) const {
    const int ix = (int)floor(x), iy = (int)floor(y);
    const double fx = smooth(x - ix), fy = smooth(y - iy);
    return lerp(lerp(grid[(iy & 255) * 256 + (ix & 255)], grid[(iy & 255) * 256 + ((ix + 1) & 255)], fx),
      lerp(grid[((iy + 1) & 255) * 256 + (ix & 255)], grid[((iy + 1) & 255) * 256 + ((ix + 1) & 255)], fx), fy);
  }
  double cloud(double x, double y) const {
    return noise(x, y) * .55 + noise(x * 2.03 + 31, y * 2.03 + 17) * .28
      + noise(x * 4.11 + 9, y * 4.11 + 51) * .17;
  }
  void starPixel(int x, int y, int r, int g, int b, double strength) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    uint32_t& p = pixels[y * width + x];
    p = pack(std::min(224, rounded(((p >> 16) & 255) + r * strength)),
      std::min(231, rounded(((p >> 8) & 255) + g * strength)),
      std::min(240, rounded((p & 255) + b * strength)));
  }
};
