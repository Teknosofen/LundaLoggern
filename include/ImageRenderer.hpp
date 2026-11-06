
#pragma once
// =============================================================================
//  ImageRenderer.hpp — everything drawn on the 320 x 170 TFT
// -----------------------------------------------------------------------------
//  Thin layer over TFT_eSPI that knows the screen layout. main.cpp decides
//  *when* to redraw (only on changes); this class knows *where* and *how*.
//
//  Layout (landscape, rotation 1, origin top-left):
//
//   +--------+  LundaLogger                       (label, large font)
//   |  logo  |                          1.3.0     (version)
//   | 100x100|  SERVO-U12345                      (device ID)
//   +--------+  2026-5-18 14:03:11                (ventilator clock)
//   +- WiFi -----------+         +- Status ---------
//   | 192.168.4.1      |   (o)   |        COM OK
//   | SSID: ...        |  phase  |        SD OK
//   | Press key to ... |   dot   |
//   +------------------+         +------------------
//
//  All text is drawn on the logo background colour; old text is erased by
//  redrawing it in the background colour, or overwritten with padded strings.
// =============================================================================
#include <TFT_eSPI.h>
#include "DateTime.hpp"


class ImageRenderer {
public:
  /// @param display TFT driver; must outlive the renderer.
  ImageRenderer(TFT_eSPI &display);

  /// Fill in all DisplayPos members below (called from begin()).
  void initPositions();
  /// "LundaLogger" title and the short version string.
  void drawLabel();
  /// Draw an RGB565 image pixel by pixel (slow; prefer pushFullImage()).
  void drawImage(int x, int y, int w, int h, const uint16_t *img);
  /// Draw an RGB565 image with a single block transfer.
  void pushFullImage(int x, int y, int w, int h, const uint16_t *img);
  // Colour swatches with a text label: palette preview helpers, unused.
  void drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label);
  void drawSwatch(int x, int y, uint16_t color, const char *label, bool  rounded = false);
  /// Green "SD OK" or red "SD FAIL" badge, right-aligned.
  void drawSDStatusIndicator(bool isPresent);
  /// Green "COM OK" or red "COM FAIL" badge (ventilator link), right-aligned.
  void drawCOMStatusIndicator(bool isPresent);
  /// Coloured dot: green = inspiration (0x10), slate = pause (0x20),
  /// red = expiration (0x30), background colour = unknown.
  void drawBreathPhase(uint8_t breathPhase);
  /// NOTE: despite the name, draws the left frame labelled "WiFi".
  void drawStatusField();
  /// NOTE: despite the name, draws the right frame labelled "Status".
  void drawWiFiField();
  /// AP IP (or "WiFi OFF") and SSID lines in the WiFi frame. Pad with spaces
  /// to overwrite longer previous text.
  void drawWiFiAPIP(String drawWiFiAPIP, String wiFiSSID);
  /// Bottom line of the WiFi frame, e.g. "Press key to enable".
  void drawWiFiPromt(String WiFiPrompt);

  /// Init the TFT, set landscape orientation, show the boot text.
  void begin();
  /// Fill the screen with the logo background colour.
  void clear();
  /// Ventilator clock below the device ID. The previous value is erased first.
  /// `spacing` is unused.
  void drawDateTimeAt(const DateTime& dt, int spacing = 20);
  /// Device type + serial number below the title.
  void drawServoID(const String& servoID);
  /// Unused early layout.
  void drawMainScreen();
  void setTextSize(int size) { tft.setTextSize(size); }
  /// Plain text with a built-in TFT_eSPI font (default font 2).
  void drawString(const String& text, int x, int y, int font = 2);
  void drawString(const char* text, int x, int y, int font = 2);


  /// Pixel position of one screen element (top-left of the text, unless noted).
  struct DisplayPos {
    int x = 0;
    int y = 0;
  };

  // Element positions, set in initPositions().
  DisplayPos logoPos;
  DisplayPos labelPos;        // "LundaLogger" title
  DisplayPos versionPos;
  DisplayPos servoIDPos;
  DisplayPos timePos;
  DisplayPos wiFiRectPos;     // left frame
  DisplayPos wiFiLabelPos;    // "WiFi" caption on the frame border
  DisplayPos wiFiAPIPPos;
  DisplayPos wiFiSSIDPos;
  DisplayPos wiFiPromptPos;
  DisplayPos statusRectPos;   // right frame
  DisplayPos statusLabelPos;  // "Status" caption on the frame border
  DisplayPos statusCOMPos;    // only y is used; x is computed (right-aligned)
  DisplayPos statusSDPos;     // only y is used; x is computed (right-aligned)
  DisplayPos phasePos;        // centre of the breath-phase dot



private:
  TFT_eSPI &tft;
  void drawCenteredText(const String& text, int y); // unused helper
  int currentTextSize = 2;
  const int smallTextSize = 1;
  const int largeTextSize = 2;
};
