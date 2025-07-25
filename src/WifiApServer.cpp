#include "WifiApServer.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include "main.hpp"

WebServer server(80);

WifiApServer::WifiApServer(String ssid, String password)
    : _ssid(ssid), _password(password),
      _logoArray(nullptr), _logoWidth(0), _logoHeight(0),
      _downloadEnabled(false), _deleteEnabled(false)
{
    for (int i = 0; i < 4; ++i) {
        _values[i] = 0.0f;
        _labels[i] = "Value " + String(i + 1);
    }
}

void WifiApServer::begin() {
    WiFi.softAP(_ssid.c_str(), _password.c_str());
    setupWebServer();
    server.begin();
}

void WifiApServer::handleClient() {
    server.handleClient();
}

void WifiApServer::setLogoData(const uint16_t* logoArray, size_t width, size_t height) {
    _logoArray = logoArray;
    _logoWidth = width;
    _logoHeight = height;
}

void WifiApServer::setTextAndValues(String text, float v1, float v2, float v3, float v4) {
    _text = text;
    _values[0] = v1; _values[1] = v2; _values[2] = v3; _values[3] = v4;
}

void WifiApServer::setValue(int index, float value) {
    if (index >= 0 && index < 4) _values[index] = value;
}

void WifiApServer::setLabel(int index, String label) {
    if (index >= 0 && index < 4) _labels[index] = label;
}

void WifiApServer::enableSdFileDownloads(bool enable) { _downloadEnabled = enable; }
void WifiApServer::enableSdFileDelete(bool enable) { _deleteEnabled = enable; }

String WifiApServer::getApIpAddress() const {
    return WiFi.softAPIP().toString();
}

void WifiApServer::setupWebServer() {
    server.on("/", [this]() { handleRoot(); });
    server.on("/files", [this]() { handleFileManager(); });
    if (_downloadEnabled)
        server.on("/download", HTTP_GET, [this]() { handleFileDownload(); });
    if (_deleteEnabled)
        server.on("/delete", HTTP_POST, [this]() { handleFileDelete(); });
}

void WifiApServer::handleRoot() {
    server.send(200, "text/html", generateHtmlPage());
}

void WifiApServer::handleFileManager() {
    std::vector<String> files = listSdFiles();
    String html = "<html><body><h2>File Manager</h2>";

    html += "<form method='POST' action='/delete'>";
    for (const auto& f : files)
        html += "<input type='checkbox' name='file' value='" + f + "'>" + f + "<br>";
    html += "<button type='submit'>Delete Selected</button></form><br>";

    html += "<form method='GET' action='/download'>";
    for (const auto& f : files)
        html += "<input type='checkbox' name='file' value='" + f + "'>" + f + "<br>";
    html += "<button type='submit'>Download Selected</button></form>";

    html += "<br><a href='/'>Back to Main Page</a></body></html>";
    server.send(200, "text/html", html);
}

void WifiApServer::handleFileDownload() {
    for (int i = 0; i < server.args(); ++i) {
        String f = "/" + server.arg(i);  // Ensure leading slash
        if (SD.exists(f.c_str())) {
            File file = SD.open(f.c_str(), FILE_READ);
            
            // Extract filename (strip path if necessary)
            String path = file.name();
            String fname = path.substring(path.lastIndexOf('/') + 1);
            
            // Set headers to trigger a download and preserve filename
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
            
            // Stream file to client
            server.streamFile(file, "application/octet-stream");
            file.close();
            return;
        }
    }

    // If no file was found
    server.send(404, "text/plain", "No file found");
}


void WifiApServer::handleFileDelete() {
    bool anyDeleted = false;
    for (int i = 0; i < server.args(); ++i) {
        String f = "/" + server.arg(i);  // 👈 Add leading slash
        hostCom.println("Deleting file: " + f);
        if (SD.exists(f.c_str())) {
            SD.remove(f.c_str());
            anyDeleted = true;
        }
    }
    server.send(200, "text/plain", anyDeleted ? "Files deleted" : "No valid files selected");
}

std::vector<String> WifiApServer::listSdFiles() {
    std::vector<String> list;
    File root = SD.open("/");
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) list.push_back(String(file.name()));
        file = root.openNextFile();
    }
    return list;
}

String WifiApServer::encodeLogoPixelsRGB888(const uint16_t* logo, int size) {
    String result = "[";
    for (int i = 0; i < size; ++i) {
        uint16_t pixel = logo[i];

        // Convert RGB565 to RGB888
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;

        result += "[" + String(r) + "," + String(g) + "," + String(b) + ",255]";
        if (i != size - 1) result += ",";
    }
    result += "]";
    return result;
}

String WifiApServer::generateHtmlPage() {
    String html = "<!DOCTYPE html><html><head><title>LundaLogger Status</title></head><body>";
    html += "<h2>" + _text + "</h2>";

    // Display system status
    html += "<ul>";
    for (int i = 0; i < 4; ++i) {
        html += "<li>" + _labels[i] + ": " + String(_values[i]) + "</li>";
    }
    html += "</ul>";

    // Logo canvas
    html += "<canvas id='logoCanvas' width='" + String(_logoWidth) + "' height='" + String(_logoHeight) + "'></canvas>";
    html += "<script>\n";
    html += "try {\n";
    html += "let canvas = document.getElementById('logoCanvas');\n";
    html += "let ctx = canvas.getContext('2d');\n";
    html += "let imageData = ctx.createImageData(" + String(_logoWidth) + ", " + String(_logoHeight) + ");\n";
    html += "let data = imageData.data;\n";

    // Embed pixel data using helper
    html += "let pixels = " + encodeLogoPixelsRGB888(lundaLogo, _logoWidth * _logoHeight) + ";\n";

    // Fill canvas pixel buffer
    html += "for (let i = 0; i < pixels.length; ++i) {\n";
    html += "  let p = pixels[i];\n";
    html += "  data[i * 4] = p[0];     // R\n";
    html += "  data[i * 4 + 1] = p[1]; // G\n";
    html += "  data[i * 4 + 2] = p[2]; // B\n";
    html += "  data[i * 4 + 3] = p[3]; // A\n";
    html += "}\n";
    html += "ctx.putImageData(imageData, 0, 0);\n";
    html += "} catch (e) {\n";
    html += "  console.error('Canvas error:', e);\n";
    html += "  document.body.innerHTML += '<p style=\"color:red;\">Logo failed to load</p>';\n";
    html += "}\n";
    html += "</script>\n";

    // File manager button
    if (_downloadEnabled || _deleteEnabled) {
        html += "<br><form method='GET' action='/files'>\n";
        html += "<button type='submit'>Open File Manager</button>\n";
        html += "</form>\n";
    }

    html += "</body></html>";
    return html;
}

// String WifiApServer::generateHtmlPage() {
//     String html = "<!DOCTYPE html><html><head><title>LundaLogger Status</title></head><body>";
//     html += "<h2>" + _text + "</h2>";

//     html += "<ul>";
//     for (int i = 0; i < 4; ++i) {
//         html += "<li>" + _labels[i] + ": " + String(_values[i]) + "</li>";
//     }
//     html += "</ul>";

//     html += "<canvas id='logoCanvas' width='" + String(_logoWidth) + "' height='" + String(_logoHeight) + "'></canvas>";
//     html += "<script>";
//     html += "let canvas = document.getElementById('logoCanvas');";
//     html += "let ctx = canvas.getContext('2d');";
//     html += "let imageData = ctx.createImageData(" + String(_logoWidth) + ", " + String(_logoHeight) + ");";
//     html += "let data = imageData.data;";

//     html += "// TODO: Fill 'data' with RGB888 pixel values derived from your lundaLogo[] array";

//     html += "ctx.putImageData(imageData, 0, 0);</script>";

//     if (_downloadEnabled || _deleteEnabled) {
//         html += "<br><form method='GET' action='/files'>";
//         html += "<button type='submit'>Open File Manager</button></form>";
//     }

//     html += "</body></html>";
//     return html;
// }