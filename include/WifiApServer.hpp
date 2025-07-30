#pragma once
#include <Arduino.h>
#include <vector>

class WifiApServer {
public:
    WifiApServer(String ssid, String password);

    void begin();
    
    void handleClient();

    void setLogoData(const uint16_t* logoArray, size_t width, size_t height);
    void setTextAndValues(String text, float v1, float v2, float v3, float v4);
    void setValue(int index, float value);
    void setLabel(int index, String label);

    void enableSdFileDownloads(bool enable = true);
    void enableSdFileDelete(bool enable = true);

    String getApIpAddress() const;

private:
    String _ssid, _password;
    const uint16_t* _logoArray;
    size_t _logoWidth, _logoHeight;
    String _text;
    float _values[4];
    String _labels[4];
    bool _downloadEnabled, _deleteEnabled;

    void setupWebServer();
    void handleRoot();
    void handleFileManager();
    void handleFileDownload();
    void handleFileDelete();
    void handleLogo();
    std::vector<String> listSdFiles();

    String encodeLogoPixelsRGB888(const uint16_t* logo, int size);
    String generateHtmlPage();
};