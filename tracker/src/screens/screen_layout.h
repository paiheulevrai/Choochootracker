#pragma once
struct AppScreen;

// The optional Song scope occupies rows 0-1. The sidebar and message row remain in physical
// screen coordinates; only the page's content uses the two-row offset.
int screenScopeRows(const AppScreen* screen);
int screenVisibleRows(void);
// Modulation uses two blocks of eight fields, with a gap between them.
int screenModulationRowY(int row);
int screenModulationRowAt(int y);

class ScreenOverlayCoordinates {
 public:
  ScreenOverlayCoordinates();
  ~ScreenOverlayCoordinates();
 private:
  int previous_;
};
