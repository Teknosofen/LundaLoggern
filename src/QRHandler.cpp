#include "QRHandler.hpp"
#include "main.hpp"

QRHandler::QRHandler() : tft(), showingHomepageQR(false) {}

void QRHandler::begin() {

    
}

void QRHandler::showWiFiQR(const char* ssid, const char* password) {
  tft.fillScreen(TFT_LOGOBACKGROUND);
  tft.drawString("Scan to connect Wi-Fi", 10, 5);

  String wifiQR = "WIFI:T:WPA;S:" + String(ssid) + ";P:" + String(password) + ";;";
  drawQRCode(wifiQR.c_str());
  showingHomepageQR = false;
}

void QRHandler::showHomepageQR(const char* url) {
  tft.fillScreen(TFT_LOGOBACKGROUND);
  tft.drawString("Open in browser:", 10, 5);
  drawQRCode(url);
  showingHomepageQR = true;
}

void QRHandler::showWiFiOff() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Wi-Fi is OFF", 40, 80);
}

bool QRHandler::isShowingHomepageQR() const {
  return showingHomepageQR;
}

void QRHandler::drawQRCode(const char *text) {
  uint8_t qrcodeData[qrcode_getBufferSize(4)];
  qrcode_initText(&qrcode, qrcodeData, 4, ECC_LOW, text);

  int scale = 6;
  int offsetX = 20;
  int offsetY = 30;

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y))
        tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, TFT_WHITE);
    }
  }
}
