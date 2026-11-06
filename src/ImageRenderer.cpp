// =============================================================================
//  ImageRenderer.cpp — TFT drawing. Layout overview in ImageRenderer.hpp.
//
//  Fonts: FSS9 / FSSB9 / FSSB18 are FreeSans (bold) free fonts from
//  Free_Fonts.h; drawString(..., 2) uses TFT_eSPI's built-in font 2.
// =============================================================================

#include "main.hpp"
#include "ImageRenderer.hpp"

ImageRenderer::ImageRenderer(TFT_eSPI &display) : tft(display) {}

// Boot screen: plain text until loop() draws the full layout on its first pass.
void ImageRenderer::begin() {
    tft.init();
    tft.setRotation(1); // landscape, 320 x 170

    tft.fillScreen(TFT_LOGOBACKGROUND);
    tft.setTextColor(TFT_WHITE, TFT_LOGOBACKGROUND  );
    currentTextSize = smallTextSize;
    tft.setTextSize(currentTextSize);
    tft.setCursor(10, 10);
    tft.println("LundaLogger Ready");
    tft.setCursor(10, 30);
    tft.println(lundaLoggerVerLbl);
    initPositions();
}

void ImageRenderer::clear() {
    tft.fillScreen(TFT_LOGOBACKGROUND);
}

// Screen coordinates of every element. Edit here to move things around.
void ImageRenderer::initPositions() {

    logoPos.x = 0;
    logoPos.y = 0;

    // Text column to the right of the logo
    labelPos.x = 90;
    labelPos.y = 10;

    versionPos.x = 290;
    versionPos.y = 45;

    servoIDPos.x = labelPos.x;
    servoIDPos.y = 60;

    timePos.x = labelPos.x;
    timePos.y = 80;

    // Left frame ("WiFi"): caption sits on the top border, three text lines inside
    wiFiRectPos.x = 5;
    wiFiRectPos.y = 100;

    wiFiLabelPos.x = wiFiRectPos.x + 5;
    wiFiLabelPos.y = wiFiRectPos.y - 10;

    wiFiAPIPPos.x = wiFiLabelPos.x;
    wiFiAPIPPos.y = wiFiLabelPos.y + 20;

    wiFiSSIDPos.x = wiFiLabelPos.x;
    wiFiSSIDPos.y = wiFiLabelPos.y + 20 + 20;

    wiFiPromptPos.x = wiFiLabelPos.x;
    wiFiPromptPos.y = wiFiLabelPos.y + 20 + 20 + 20;

    // Right frame ("Status"); it is 150 px wide, so it runs past the right
    // screen edge, which leaves it open on that side.
    statusRectPos.x = 205;
    statusRectPos.y = 100;

    statusLabelPos.x = statusRectPos.x + 5 ;
    statusLabelPos.y = statusRectPos.y - 10 ;

    statusCOMPos.x = 0;               // will be overridden by function
    statusCOMPos.y = 108;

    statusSDPos.x = 0;                // will be overridden by function
    statusSDPos.y = 138;

    // Breath-phase dot, between the two frames
    phasePos.x = 180;
    phasePos.y = 140;
}

// Unused early version of the title drawing; see drawLabel().
void ImageRenderer::drawMainScreen() {
    tft.setFreeFont(FSSB18);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(smallTextSize);
    tft.setCursor(10, 10);
    tft.drawString("LundaLogger", 10, 10);
    tft.setCursor(10, 50);
    tft.setTextSize(smallTextSize);
};

void ImageRenderer::drawLabel() {
    tft.setFreeFont(FSSB18);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(smallTextSize);
    tft.drawString("LundaLogger", labelPos.x, labelPos.y);
    tft.setFreeFont(FSS9);
    tft.drawString(VERSION, versionPos.x, versionPos.y, 2);
}

// NOTE: the names of drawStatusField() and drawWiFiField() are swapped
// relative to what they draw. This one draws the LEFT frame captioned "WiFi".
void ImageRenderer::drawStatusField() {
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawRoundRect(wiFiRectPos.x, wiFiRectPos.y, 150, 70, 10, TFT_WHITE);
    tft.drawString("WiFi  ", wiFiLabelPos.x, wiFiLabelPos.y);  // caption on the top border
}
// ...and this one draws the RIGHT frame captioned "Status".
void ImageRenderer::drawWiFiField() {
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(smallTextSize);
    tft.drawRoundRect(statusRectPos.x, statusRectPos.y, 150, 70, 10, TFT_WHITE);
    tft.drawString("Status  ", statusLabelPos.x, statusLabelPos.y);
}

// Drawn with built-in font 2. Callers pad the strings with trailing spaces so
// that a shorter text covers a longer previous one.
void ImageRenderer::drawWiFiAPIP(String WiFiAPIP, String wiFiSSID) {
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawString(WiFiAPIP, wiFiAPIPPos.x, wiFiAPIPPos.y, 2);
    tft.setTextSize(1);
    tft.drawString("SSID: " + wiFiSSID, wiFiSSIDPos.x, wiFiSSIDPos.y, 2);
}

void ImageRenderer::drawWiFiPromt(String WiFiPrompt) {
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawString(WiFiPrompt,  wiFiPromptPos.x, wiFiPromptPos.y , 2);
}


void ImageRenderer::drawString(const String& text, int x, int y, int font) {
    tft.drawString(text, x, y, font);
}

void ImageRenderer::drawString(const char* text, int x, int y, int font) {
      drawString(String(text), x, y, font);
}

void ImageRenderer::drawCenteredText(const String& text, int y) {
    int x = (tft.width() - tft.textWidth(text)) / 2;
    tft.drawString(text, x, y);
}

// Meant as erase-then-draw, like drawDateTimeAt(). But prevIDText is a local
// that starts empty on every call, so the erase step prints nothing and the
// previous ID is not removed before the new one is drawn.
void ImageRenderer::drawServoID(const String& servoID) {
  String prevIDText = "";
  tft.setFreeFont(FSS9);
  tft.setTextSize(smallTextSize);

  tft.setCursor(servoIDPos.x, servoIDPos.y);
  tft.setTextColor(TFT_LOGOBACKGROUND, TFT_LOGOBACKGROUND);
  tft.print(prevIDText);

  tft.setCursor(servoIDPos.x, servoIDPos.y);
  tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
  tft.print(servoID);
  prevIDText = servoID;
}

// Called every 200 ms. Free fonts do not paint a text background, so the
// previous string is erased by printing it again in the background colour
// before the new one is drawn. Format: "YYYY-M-D H:MM:SS" (date and hour are
// not zero-padded).
void ImageRenderer::drawDateTimeAt(const DateTime& dt, int spacing) {
    static String prevDateTimeStr = "";   // what is currently on screen

    uint16_t textColor = TFT_DEEPBLUE;
    uint16_t bgColor   = TFT_LOGOBACKGROUND;
    tft.setFreeFont(FSS9);

    int prevFontSize = smallTextSize; // text size to restore at the end
    int smallFont = 1;

    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(smallFont);

    if (!dt.isValid()) {
      // Clock not set yet (no ventilator contact since boot).
      tft.setCursor(timePos.x, timePos.y);
      tft.print("Invalid Time");

      prevDateTimeStr = "Invalid Time";
      return;
    }

    String dateStr = String(dt.year()) + "-" +
                     String(dt.month()) + "-" +
                     String(dt.day()) + " ";

    String timeStr = String(dt.hour()) + ":" +
                     (dt.minute() < 10 ? "0" : "") + String(dt.minute()) + ":" +
                     (dt.second() < 10 ? "0" : "") + String(dt.second());

    // Erase previous date/time
    tft.setTextColor(bgColor, bgColor);
    tft.setCursor(timePos.x, timePos.y);
    tft.print(prevDateTimeStr);

    // Draw new date/time
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(timePos.x, timePos.y);
    tft.print(dateStr + timeStr);

    prevDateTimeStr = dateStr + timeStr;

    tft.setTextSize(prevFontSize);
}

void ImageRenderer::drawImage(int x, int y, int w, int h, const uint16_t *img) {
  tft.startWrite();   // keep the SPI transaction open for all pixels
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t color = img[row * w + col];
      tft.drawPixel(x + col, y + row, color);
    }
  }
  tft.endWrite();
}

void ImageRenderer::pushFullImage(int x, int y, int w, int h, const uint16_t *img) {
  tft.pushImage(x, y, w, h, img);
}

// Fixed-size swatch with the label roughly centred vertically.
void ImageRenderer::drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label) {
  tft.fillRect(x, y, width, height, color);
  tft.setTextColor(TFT_WHITE, color);
  tft.setCursor(x + 5, y + (height / 2) - 8);
  tft.print(label);
}

// Swatch sized to fit its label, optionally with rounded corners.
void ImageRenderer::drawSwatch(int x, int y, uint16_t color, const char *label, bool rounded) {
  int textWidth  = tft.textWidth(label);
  int textHeight = tft.fontHeight();

  int padding = 10;
  int boxWidth  = textWidth + padding * 2;
  int boxHeight = textHeight + padding;

  if (rounded) {
    int radius = 5;
    tft.fillRoundRect(x, y, boxWidth, boxHeight, radius, color);
  } else {
    tft.fillRect(x, y, boxWidth, boxHeight, color);
  }

  tft.setTextColor(TFT_WHITE, color);
  tft.setCursor(x + padding, y + (boxHeight - textHeight) / 2);
  tft.print(label);
}


// Status badge for the SD card. The box is sized for the longest text
// ("SD FAIL") so both states cover exactly the same area. The text datum and
// colours are restored afterwards; the font is not.
void ImageRenderer::drawSDStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "SD OK" : "SD FAIL";

  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  tft.setFreeFont(FSSB9);
  tft.setTextColor(color, backColor);
  tft.setTextDatum(MC_DATUM);                // text is positioned by its centre

  int textWidth  = tft.textWidth("SD FAIL"); // size for the longest message
  int textHeight = tft.fontHeight();
  const int paddingX = 3;
  const int paddingY = 3;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  // TFT_HEIGHT is the panel's long side (320), i.e. the screen width in landscape.
  int x = TFT_HEIGHT - boxWidth - 10;        // right-aligned, 10 px margin
  int y = statusSDPos.y = 138;               // lower badge in the Status frame
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);

  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);

}

// Status badge for the ventilator link; same layout as the SD badge, upper row.
void ImageRenderer::drawCOMStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "COM OK" : "COM FAIL";

  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  tft.setFreeFont(FSSB9);
  tft.setTextColor(color, backColor);
  tft.setTextDatum(MC_DATUM);

  int textWidth  = tft.textWidth("COM FAIL"); // size for the longest message
  int textHeight = tft.fontHeight();
  const int paddingX = 3;
  const int paddingY = 3;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  int x = TFT_HEIGHT - boxWidth - 10;        // right-aligned, see drawSDStatusIndicator()
  int y = statusCOMPos.y;                    // upper badge in the Status frame
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);

  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);
}

// Breath-phase dot. Phase byte values come from the SCI curve stream
// (0x81 phase flag, see ServoCIEData). main.cpp only calls this on a change.
void ImageRenderer:: drawBreathPhase(uint8_t phase) {
  static String prevPhase = "";
  String phaseString = "";
  uint16_t textColor = TFT_DEEPBLUE;
  uint16_t bgColor   = TFT_LOGOBACKGROUND;
  tft.setFreeFont(FSS9);

  int prevFontSize = smallTextSize;
  int smallFont = 1;

  #define inspPhase 0x10
  #define pausePhase 0x20
  #define expPhase 0x30

  const int centerX = phasePos.x;
  const int centerY = phasePos.y;
  const int radius = 15;

  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(smallFont);

  uint16_t color;
  switch (phase) {
    case inspPhase:
      color = TFT_GREENISH_TINT ;   // Inhale
    break;
    case pausePhase:
      case 1: color = TFT_SLATEBLUE ;  // Hold (a phase value of 1 is also shown as hold)
    break;
    case expPhase:
      color = TFT_REDDISH_TINT;    // Exhale
    break;
    default:
      color = TFT_LOGOBACKGROUND;  // Unknown: dot disappears
    break;
  }
  tft.fillCircle(phasePos.x, phasePos.y, radius, color);
    prevPhase = phaseString;   // leftover from an earlier text version; unused
  }
