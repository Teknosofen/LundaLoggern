#ifndef QRHANDLER_HPP
#define QRHANDLER_HPP
// =============================================================================
//  QRHandler.hpp — full-screen QR codes for joining WiFi / opening the web page
// -----------------------------------------------------------------------------
//  Implemented but NOT used: nothing in main.cpp creates a QRHandler.
//  Before wiring it in, note that it owns a second TFT_eSPI instance; it
//  should rather draw through the global `tft`/ImageRenderer.
// =============================================================================

#include <TFT_eSPI.h>
#include <qrcode.h>

class QRHandler {
public:
  QRHandler();

  /// Empty; reserved for initialisation.
  void begin();
  /// QR code in the standard WiFi format, so a phone camera offers to join the network.
  void showWiFiQR(const char* ssid, const char* password);
  /// QR code for a URL, e.g. "http://192.168.4.1/".
  void showHomepageQR(const char* url);
  /// Full-screen "Wi-Fi is OFF" message.
  void showWiFiOff();

  /// True while the homepage QR (not the WiFi QR) is on screen.
  bool isShowingHomepageQR() const;

private:
  TFT_eSPI tft;
  QRCode qrcode;
  bool showingHomepageQR;

  /// Draw `text` as a version-4 QR code (33 x 33 modules, 6 px each).
  void drawQRCode(const char *text);
};

#endif
