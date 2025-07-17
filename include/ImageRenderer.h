
#pragma once
#include <TFT_eSPI.h>

class ImageRenderer {
public:
  ImageRenderer(TFT_eSPI &display) : tft(display) {}

  void drawImage(int x, int y, int w, int h, const uint16_t *img) {
    tft.startWrite();
    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        uint16_t color = img[row * w + col];
        tft.drawPixel(x + col, y + row, color);
      }
    }
    tft.endWrite();
  }

  // Optional: use pushImage for faster rendering when possible
  void pushFullImage(int x, int y, int w, int h, const uint16_t *img) {
    tft.pushImage(x, y, w, h, img);
  }

private:
  TFT_eSPI &tft;
};