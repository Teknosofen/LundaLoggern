#include "main.hpp"
#include "ImageRenderer.hpp"

ImageRenderer::ImageRenderer(TFT_eSPI &display) : tft(display) {}

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

// void ImageRenderer::drawSDStatusIndicator(bool isPresent) {  
//     const uint16_t color     = isPresent ? 0x07E0 : 0xF800; // Green or Red
//     const uint16_t BackColor = isPresent ? 0x0330 : 0x4000; // contrasting Green or Red
//     const char* label = isPresent ? "SD OK" : "SD FAIL";
//     tft.setFreeFont(FSSB12); // Set a larger font for better visibility
    
//     int textWidth  = tft.textWidth("SD FAIL"); // adopt to the longest message
//     int textHeight = tft.fontHeight();
//     const int padding = 6;
//     int boxWidth  = textWidth + padding * 2;
//     int boxHeight = textHeight + padding;

//     int x = TFT_HEIGHT - textWidth - 15; // Right-aligned with margin
//     int y = 140; // Vertical position under logo; tweak if needed

//     tft.setTextColor(color, BackColor); // Set text color and background
//     // tft.fillRect(x, y, boxWidth, boxHeight, BackColor); // Clear area for text
//     int radius = 5;
//     tft.fillRoundRect(x, y, boxWidth, boxHeight, radius, BackColor);
//     tft.setCursor(x + padding, y + (boxHeight - textHeight) / 2);
//     tft.print(label);
    
// }
