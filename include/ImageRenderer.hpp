
#pragma once
#include <TFT_eSPI.h>

#pragma once
#include <TFT_eSPI.h>

class ImageRenderer {
public:
  ImageRenderer(TFT_eSPI &display);

  void drawImage(int x, int y, int w, int h, const uint16_t *img);
  void pushFullImage(int x, int y, int w, int h, const uint16_t *img);
  void drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label);
  void drawSwatch(int x, int y, uint16_t color, const char *label, bool  rounded = false);
  void drawSDStatusIndicator(bool isPresent);

private:
  TFT_eSPI &tft;
};




// class ImageRenderer {
// public:
//   ImageRenderer(TFT_eSPI &display) : tft(display) {}

//   void drawImage(int x, int y, int w, int h, const uint16_t *img) {
//     tft.startWrite();
//     for (int row = 0; row < h; row++) {
//       for (int col = 0; col < w; col++) {
//         uint16_t color = img[row * w + col];
//         tft.drawPixel(x + col, y + row, color);
//       }
//     }
//     tft.endWrite();
//   }

//   // Optional: use pushImage for faster rendering when possible
//   void pushFullImage(int x, int y, int w, int h, const uint16_t *img) {
//     tft.pushImage(x, y, w, h, img);
//   }

//   void ImageRenderer::drawSwatch(int x, int y, int width, int height, uint16_t color, const char* label) {
//   tft.fillRect(x, y, width, height, color);
//   tft.setTextColor(TFT_WHITE, color); // White text on color background
//   tft.setCursor(x + 5, y + (height / 2) - 8); // Vertically center the text
//   tft.print(label);
// }
// '
// private:
//   TFT_eSPI &tft;
// };