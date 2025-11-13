#ifndef QRHANDLER_HPP
#define QRHANDLER_HPP

#include <TFT_eSPI.h>
#include <qrcode.h>

class QRHandler {
public:
  QRHandler();

  void begin();
  void showWiFiQR(const char* ssid, const char* password);
  void showHomepageQR(const char* url);
  void showWiFiOff();

  bool isShowingHomepageQR() const;

private:
  TFT_eSPI tft;
  QRCode qrcode;
  bool showingHomepageQR;

  void drawQRCode(const char *text);
};

#endif
