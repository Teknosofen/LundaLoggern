#include "main.hpp"
#include "ImageRenderer.hpp"

ImageRenderer::ImageRenderer(TFT_eSPI &display) : tft(display) {}

void ImageRenderer::begin() {
    tft.init();
    tft.setRotation(1); // Set display orientation
    
    tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display
    tft.setTextColor(TFT_WHITE, TFT_LOGOBACKGROUND  ); // Set text color and background
    currentTextSize = smallTextSize;
    tft.setTextSize(currentTextSize); // Set text size
    tft.setCursor(10, 10); // Set cursor position
    tft.println("LundaLogger Ready"); // Print a message on the display
    delay(1000); // Delay to allow the display to show the message
    // tft.setTextDatum(TL_DATUM);  // Top-left for manual positioning
    pushFullImage(220, 40, 100, 100, lundaLogo);
}
    
void ImageRenderer::clear() {
    tft.fillScreen(TFT_LOGOBACKGROUND);
}

void ImageRenderer::drawMainScreen() {
  /// screen gfx stuff
    tft.setFreeFont(FSSB18);    
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
    tft.setTextSize(smallTextSize); // Set text size
    tft.setCursor(10, 10); // Set cursor position
    tft.drawString("LundaLogger", 10, 10); // Print a message on the display
    tft.setCursor(10, 50); // Set cursor position for next line
    tft.setTextSize(smallTextSize); // Set text size for the next line
};

void ImageRenderer::drawString(const String& text, int x, int y, int font) {
    tft.drawString(text, x, y, font);
}

void ImageRenderer::drawString(const char* text, int x, int y, int font) {
    drawString(String(text), x, y, font);  // Delegate to String version
}

void ImageRenderer::drawCenteredText(const String& text, int y) {
    int x = (tft.width() - tft.textWidth(text)) / 2;
    tft.drawString(text, x, y);
}

void ImageRenderer::drawServoID(const String& servoID, int x, int y) {
    tft.setFreeFont(FSS9);  
    
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(smallTextSize);

    // Draw new date/time in small font
    tft.setCursor(x, y);
    tft.print(servoID);
}

void ImageRenderer::drawDateTimeAt(const DateTime& dt, int x, int y, int spacing) {
    static String prevDateTimeStr = "";
    
    uint16_t textColor = TFT_DEEPBLUE;
    uint16_t bgColor   = TFT_LOGOBACKGROUND;
    tft.setFreeFont(FSS9);  

    int prevFontSize = smallTextSize; // save current font size
    int smallFont = 1;                  // smaller font for date/time
    
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(smallFont);

    if (!dt.isValid()) {
      tft.setCursor(x, y);
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
    tft.setCursor(x, y);
    tft.print(prevDateTimeStr);

    // Draw new date/time in small font
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(x, y);
    tft.print(dateStr + timeStr);

    // Save for next update
    prevDateTimeStr = dateStr + timeStr;
    
    // Restore previous font size
    tft.setTextSize(prevFontSize);
}

void ImageRenderer::drawImage(int x, int y, int w, int h, const uint16_t *img) {
  tft.startWrite();
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

void ImageRenderer::drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label) {
  tft.fillRect(x, y, width, height, color);
  tft.setTextColor(TFT_WHITE, color); // White text on color background
  tft.setCursor(x + 5, y + (height / 2) - 8); // Vertically center the text
  tft.print(label);
}

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


void ImageRenderer::drawSDStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "SD OK" : "SD FAIL";

  // 🔧 Backup current font settings
  // const GFXfont* prevFont = tft.getFreeFont();
  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  // 🖋️ Apply swatch-specific font settings
  tft.setFreeFont(FSSB9);                    // Larger, readable font
  tft.setTextColor(color, backColor);        // Foreground & background
  tft.setTextDatum(MC_DATUM);                // Middle Center alignment

  int textWidth  = tft.textWidth("SD FAIL"); // adopt to the longest message
  int textHeight = tft.fontHeight();         // Font height
  const int paddingX = 4;
  const int paddingY = 4;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  int x = TFT_HEIGHT - boxWidth - 10;        // Right-aligned
  int y = 135;                                // Position under logo
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);             // Centered text

  // 🔄 Restore previous font settings
  // tft.setFreeFont(prevFont);
  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);

}

void ImageRenderer::drawCOMStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "COM OK" : "COM FAIL";

  // 🔧 Backup current font settings
  // const GFXfont* prevFont = tft.getFreeFont();
  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  // 🖋️ Apply swatch-specific font settings
  tft.setFreeFont(FSSB9);                    // Larger, readable font
  tft.setTextColor(color, backColor);        // Foreground & background
  tft.setTextDatum(MC_DATUM);                // Middle Center alignment

  int textWidth  = tft.textWidth("COM FAIL"); // adopt to the longest message
  int textHeight = tft.fontHeight();         // Font height
  const int paddingX = 4;
  const int paddingY = 4;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  int x = 10; //         // left-aligned
  int y = 90;                                // Position under logo
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);             // Centered text

  // 🔄 Restore previous font settings
  // tft.setFreeFont(prevFont);
  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);
}

void ImageRenderer:: drawBreathPhase(uint8_t phase, int x, int y) {
  static String prevPhase = "";
  String phaseString = "";
  uint16_t textColor = TFT_DEEPBLUE;
  uint16_t bgColor   = TFT_LOGOBACKGROUND;
  tft.setFreeFont(FSS9);  

  int prevFontSize = smallTextSize; // save current font size
  int smallFont = 1;                  // smaller font for date/time
    
  #define inspPhase 0x10
  #define pausePhase 0x20
  #define expPhase 0x30

  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(smallFont);

  switch (phase) {
    case inspPhase:
      phaseString = "IN ";
    break;  
    case pausePhase:
      phaseString = " - ";
    break;  
    case expPhase:
      phaseString = "OUT";
    break;  
    default:
      phaseString = "---";
    break;
  }
    // hostCom.println(phaseString);

    // Erase previous date/time
    tft.setTextColor(bgColor, bgColor);
    tft.setCursor(x, y);
    tft.print(prevPhase);

    // Draw new phase
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(x, y);
    tft.print(phaseString);
    prevPhase = phaseString;
  }
