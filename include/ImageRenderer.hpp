
#pragma once
#include <TFT_eSPI.h>
#include "DateTime.hpp"


class ImageRenderer {
public:
  ImageRenderer(TFT_eSPI &display);

  void drawImage(int x, int y, int w, int h, const uint16_t *img);
  void pushFullImage(int x, int y, int w, int h, const uint16_t *img);
  void drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label);
  void drawSwatch(int x, int y, uint16_t color, const char *label, bool  rounded = false);
  void drawSDStatusIndicator(bool isPresent);

  void begin();                     // Initialize display
  void clear();                     // Clear screen
  void drawDateTimeAt(const DateTime& dt, int x, int y, int spacing = 20); // Draw date/time at position
  void drawServoID(const String& servoID, int x, int y);
  void drawMainScreen();        // Draw main screen layout
  void setTextSize(int size) { tft.setTextSize(size); } // Set text size
  void drawString(const String& text, int x, int y, int font = 2);
  void drawString(const char* text, int x, int y, int font = 2);

private:
  TFT_eSPI &tft;
  void drawCenteredText(const String& text, int y); // Optional helper
  int currentTextSize = 2;
  const int smallTextSize = 1;
  const int largeTextSize = 2;
};
