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
   
    const uint16_t color = isPresent ? 0x07E0 : 0xF800; // Green or Red
    const char* label = isPresent ? "SD OK" : "SD FAIL";
    
    int textWidth  = tft.textWidth("SD FAIL"); // adopt to the longest message
    int textHeight = tft.fontHeight();
    const int padding = 10;
    int boxWidth  = textWidth + padding * 2;
    int boxHeight = textHeight + padding;

    int x = TFT_HEIGHT - textWidth - 10; // Right-aligned with margin
    int y = 140; // Vertical position under logo; tweak if needed

    tft.setTextColor(color, TFT_LOGOBACKGROUND); // Set text color and background
    tft.fillRect(x, y, boxWidth, boxHeight, TFT_LOGOBACKGROUND); // Clear area for text
    tft.setCursor(x + padding, y + (boxHeight - textHeight) / 2);
    tft.print(label);
}
