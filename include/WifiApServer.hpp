#pragma once
// =============================================================================
//  WifiApServer.hpp — WiFi access point and web interface
// -----------------------------------------------------------------------------
//  Runs the ESP32 as a WiFi access point (SoftAP, default IP 192.168.4.1) with
//  a small web server on port 80. WiFi is off at boot; main.cpp calls begin()
//  on a long button press and shuts the radio down on a short press.
//
//  Routes:
//      /          dashboard: SD status, logo, label list         (no login)
//      /logo      logo PNG from SPIFFS                            (no login)
//      /files     file manager: list SD files, select, act        (login)
//      /download  GET  ?file=<name> - download a file             (login, if enabled)
//      /delete    POST  file=<name>... - delete files              (login, if enabled)
//      /configs   show the SPIFFS copies of the config files       (login)
//  "login" = HTTP basic auth with LOGGER_WEB_USER / LOGGER_WEB_PW (main.hpp).
//
//  Pages are built as HTML strings on each request; there is no file system
//  based web content apart from the logo.
// =============================================================================
#include <Arduino.h>
#include <vector>

class WifiApServer {
public:
    /// Store the AP credentials. Nothing is started until begin().
    WifiApServer(String ssid, String password);

    /// Start the access point, register the routes and start the HTTP server.
    /// Call enableSdFileDownloads()/enableSdFileDelete() before the first call.
    /// May be called again after end(); the routes are only registered once.
    void begin();

    /// Stop the HTTP server and the access point, and switch the WiFi radio off.
    void end();

    /// Serve pending HTTP requests. Call from loop() on every pass.
    void handleClient();

    void setLogoData(const uint16_t* logoArray, size_t width, size_t height); // declared, not implemented
    /// Set the dashboard text and the four values (values are not shown, see generateHtmlPage()).
    void setTextAndValues(String text, float v1, float v2, float v3, float v4);
    /// Set the dashboard text. Stored, but not currently rendered on any page.
    void setText(String text);
    void setValue(int index, float value);
    /// Set one of the four dashboard list lines (index 0..3). Out-of-range indexes are ignored.
    void setLabel(int index, String label);

    /// Enable the /download and /delete routes. Only takes effect if called
    /// before the first begin(), when the routes are registered.
    void enableSdFileDownloads(bool enable = true);
    void enableSdFileDelete(bool enable = true);

    /// Access point IP as text, e.g. "192.168.4.1".
    String getApIpAddress() const;

    /// Load an image file from SPIFFS into PSRAM (currently unused).
    bool loadLogoToPsram(const char* path);
    /// Free the buffer allocated by loadLogoToPsram().
    void freePsramImage();

private:
    String _ssid, _password;
    const uint16_t* _logoArray;       // unused
    size_t _logoWidth, _logoHeight;   // unused
    String _text;
    float _values[4];
    String _labels[4];                // dashboard list lines
    bool _downloadEnabled, _deleteEnabled;
    bool _routesRegistered = false;   // set by the first begin()

    void setupWebServer();
    /// HTTP basic auth check. If it fails, a login request has already been sent.
    bool ensureAuthenticated();
    void handleRoot();
    void handleFileManager();
    void handleFileDownload();
    void handleFileDelete();
    void handleLogo();
    void handleConfigViewer();
    std::vector<String> listSdFiles(); // unused

    String encodeLogoPixelsRGB888(const uint16_t* logo, int size); // declared, not implemented
    String generateHtmlPage();
    /// Escape &, <, >, " and ' so text can be placed in HTML safely.
    String htmlEscape(const String& text);
    /// Request argument `index` as a plain file name in the SD root, or "" if it
    /// could escape the root (path separators, "..") or contains control/quote characters.
    String safeFileArg(int index);

    uint8_t* imageBuffer = nullptr;  // image data in PSRAM, see loadLogoToPsram()
    size_t imageSize = 0;            // size of the image in bytes

};
