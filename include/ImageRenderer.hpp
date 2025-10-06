
#pragma once
#include <TFT_eSPI.h>
#include "DateTime.hpp"


class ImageRenderer {
public:
  ImageRenderer(TFT_eSPI &display);

  void initPositions();
  void drawLabel();
  void drawImage(int x, int y, int w, int h, const uint16_t *img);
  void pushFullImage(int x, int y, int w, int h, const uint16_t *img);
  void drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label);
  void drawSwatch(int x, int y, uint16_t color, const char *label, bool  rounded = false);
  void drawSDStatusIndicator(bool isPresent);
  void drawCOMStatusIndicator(bool isPresent);
  void drawBreathPhase(uint8_t breathPhase);
  void drawStatusField();
  void drawWiFiField();
  void drawWiFiAPIP(String drawWiFiAPIP);

  void begin();                     // Initialize display
  void clear();                     // Clear screen
  void drawDateTimeAt(const DateTime& dt, int spacing = 20); // Draw date/time at position
  void drawServoID(const String& servoID);
  void drawMainScreen();        // Draw main screen layout
  void setTextSize(int size) { tft.setTextSize(size); } // Set text size
  void drawString(const String& text, int x, int y, int font = 2);
  void drawString(const char* text, int x, int y, int font = 2);


  struct DisplayPos {
    int x = 0;
    int y = 0;
  };

  DisplayPos logoPos;
  DisplayPos labelPos;
  DisplayPos versionPos;
  DisplayPos servoIDPos;
  DisplayPos timePos;
  DisplayPos wiFiRectPos;
  DisplayPos wiFiLabelPos;
  DisplayPos wiFiAPIPPos;
  DisplayPos wiFiPromptPos;
  DisplayPos statusRectPos;
  DisplayPos statusLabelPos;
  DisplayPos statusCOMPos;
  DisplayPos statusSDPos;
  DisplayPos phasePos;



private:
  TFT_eSPI &tft;
  void drawCenteredText(const String& text, int y); // Optional helper
  int currentTextSize = 2;
  const int smallTextSize = 1;
  const int largeTextSize = 2;
};
