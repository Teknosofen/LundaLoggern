#include "WifiApServer.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPIFFS.h>
#include "main.hpp"

extern SDManager sd;
extern WifiApServer WiFiserver;

WebServer server(80);

WifiApServer::WifiApServer(String ssid, String password)
    : _ssid(ssid), _password(password),
      _downloadEnabled(false), _deleteEnabled(false)
{
    for (int i = 0; i < 4; ++i) {
        _values[i] = 0.0f;
        _labels[i] = "Value " + String(i + 1);
    }
}

void WifiApServer::begin() {
    WiFi.softAP(_ssid.c_str(), _password.c_str());

    // if (!SPIFFS.begin(true)) {
    //     hostCom.println("Failed to mount SPIFFS");
    // }
    setupWebServer();
    server.begin();
    
    // if (!loadLogoToPsram("/LogoWeb.png")) {
    //     Serial.println("⚠️ Logo image not loaded.");
    // }
}

void WifiApServer::handleClient() {
    server.handleClient();
}

void WifiApServer::setText(String text) {
    _text = text;
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
    server.on("/logo", HTTP_GET, [this]() { handleLogo(); });
    
    // ✅ New route for config viewer
    server.on("/configs", [this]() { handleConfigViewer(); });

    if (_downloadEnabled)
        server.on("/download", HTTP_GET, [this]() { handleFileDownload(); });
    if (_deleteEnabled)
        server.on("/delete", HTTP_POST, [this]() { handleFileDelete(); });
}

void WifiApServer::handleRoot() {
    server.send(200, "text/html", generateHtmlPage());
}

void WifiApServer::handleFileManager() {
    File root = SD.open("/");
    File file = root.openNextFile();

    String html = "<!DOCTYPE html><html><head><title>File Manager</title>";
    html += "<style>";
    html += "body { background-color: #84B6D6; font-family: Arial, sans-serif; color: #000; margin: 20px; }";
    html += "h2 { text-align: center; }";
    html += "table { border-collapse: collapse; width: 100%; max-width: 700px; margin: 0 auto; background-color: #fff; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 6px rgba(0,0,0,0.15); }";
    html += "th, td { padding: 10px; border-bottom: 1px solid #ccc; text-align: left; }";
    html += "th { background-color: #e0f8e0; color: #207520; font-weight: bold; }";
    html += "tr:nth-child(even) { background-color: #f9f9f9; }";


    html += "button { background-color: #0078D7; color: white; border: none; padding: 10px 20px; border-radius: 10px; cursor: pointer; font-size: 15px; box-shadow: 2px 2px 6px rgba(0,0,0,0.3); }";
    html += "button:hover { background-color: #005FA3; }";
    html += "form { text-align: center; margin-top: 15px; }";
    html += "a { color: #000; text-decoration: none; }";
    html += "</style>";

    // --- JavaScript ---
    html += "<script>";
    html += "function confirmDelete(event) {";
    html += "  if (!confirm('Are you sure you want to delete the selected files?')) {";
    html += "    event.preventDefault();";
    html += "  }";
    html += "}";
    html += "function toggleAll(source) {";
    html += "  let checkboxes = document.querySelectorAll('#fileTable input[type=checkbox]');";
    html += "  for (let cb of checkboxes) { cb.checked = source.checked; }";
    html += "}";
    html += "function copySelection(formId) {";  // copy selected checkboxes to hidden inputs
    html += "  let form = document.getElementById(formId);";
    html += "  form.querySelectorAll('input[name=file]').forEach(e => e.remove());";  // clear old
    html += "  document.querySelectorAll('#fileTable input[name=file]:checked').forEach(cb => {";
    html += "    let hidden = document.createElement('input');";
    html += "    hidden.type = 'hidden';";
    html += "    hidden.name = 'file';";
    html += "    hidden.value = cb.value;";
    html += "    form.appendChild(hidden);";
    html += "  });";
    html += "}";
    html += "</script></head><body>";

    // --- Page content ---
    html += "<h2>File Manager</h2>";

    // Table (not inside any form)
    html += "<table id='fileTable'><tr>";
    html += "<th><input type='checkbox' onclick='toggleAll(this)'></th>";
    html += "<th>File</th><th>Size (KB)</th></tr>";

    while (file) {
        if (!file.isDirectory()) {
            String name = String(file.name());
            size_t sizeKB = file.size() / 1024;
            html += "<tr>";
            html += "<td><input type='checkbox' name='file' value='" + name + "'></td>";
            html += "<td>" + name + "</td>";
            html += "<td>" + String(sizeKB) + "</td>";
            html += "</tr>";
        }
        file = root.openNextFile();
    }

    html += "</table><br>";

    // --- Buttons (same style as on main page) ---
    html += "<form id='downloadForm' method='GET' action='/download' onsubmit='copySelection(\"downloadForm\")'>";
    html += "<button type='submit'>Download Selected</button>";
    html += "</form>";

    html += "<form id='deleteForm' method='POST' action='/delete' onsubmit='copySelection(\"deleteForm\"); confirmDelete(event)'>";
    html += "<button type='submit'>Delete Selected</button>";
    html += "</form>";

    html += "<form method='GET' action='/'>";
    html += "<button type='submit'>Back to Main Page</button>";
    html += "</form>";


    html += "</body></html>";

    server.send(200, "text/html", html);
}


void WifiApServer::handleFileDownload() {
    for (int i = 0; i < server.args(); ++i) {
        String f = "/" + server.arg(i);  // Ensure leading slash
        if (SD.exists(f.c_str())) {
            File file = SD.open(f.c_str(), FILE_READ);

            // Extract filename
            String path = file.name();
            String fname = path.substring(path.lastIndexOf('/') + 1);

            // Trigger browser download
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
            server.streamFile(file, "application/octet-stream");
            file.close();
            return;
        }
    }
    server.send(404, "text/plain", "No file found");
}

void WifiApServer::handleFileDelete() {
    for (int i = 0; i < server.args(); ++i) {
        String f = "/" + server.arg(i);
        hostCom.println("Deleting file: " + f);
        if (SD.exists(f.c_str())) {
            SD.remove(f.c_str());
        }
    }

    // ✅ Redirect to file manager to show updated list (prevents stale checkboxes)
    server.sendHeader("Location", "/files", true);
    server.send(302, "text/plain", "");
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

// void WiFiAPServer::initLogo() [];

void WifiApServer::handleLogo() {
    File file = SPIFFS.open("/LogoWeb.png", "r");
    if (!file) {
        server.send(404, "text/plain", "Logo not found");
        return;
    }
    server.streamFile(file, "image/png");
    file.close();
}

void WifiApServer::handleConfigViewer() {
    String html = "<!DOCTYPE html><html><head><title>Configuration Files</title>";
    
    // 🌈 Apply unified color theme and soft styling
    html += "<style>";
    html += "body { background-color: #84B6D6; font-family: Arial, sans-serif; color: #000; margin: 20px; }";
    html += "h2, h3 { text-align: center; }";


    html += "pre { background-color: #fff; padding: 10px; border: 1px solid #ccc; border-radius: 6px; max-width: 800px; margin: 10px auto; white-space: pre-wrap; box-shadow: 2px 2px 6px rgba(0,0,0,0.2); }";
    html += "button { background-color: #0078D7; color: white; border: none; padding: 10px 20px; border-radius: 10px; cursor: pointer; font-size: 15px; box-shadow: 2px 2px 6px rgba(0,0,0,0.3); }";
    html += "button:hover { background-color: #005FA3; }";
    html += "form { text-align: center; margin-top: 20px; }";
    html += "</style></head><body>";
    
    html += "<h2>Configuration Files</h2>";

    // --- Inline helper to read file content ---
    auto readFileContent = [](const char* path) -> String {
        File file = SPIFFS.open(path, "r");
        if (!file) {
            return String("[Error: Could not open ") + path + "]";
        }
        String content;
        while (file.available()) {
            content += (char)file.read();
        }
        file.close();
        return content;
    };

    // --- MetricConfig.txt ---
    html += "<h3>MetricConfig.txt</h3>";
    html += "<pre>" + readFileContent("/MetricConfig.txt") + "</pre>";

    // --- SettingConfig.txt ---
    html += "<h3>SettingConfig.txt</h3>";
    html += "<pre>" + readFileContent("/SettingConfig.txt") + "</pre>";

    // --- Back Button ---
    html += "<form method='GET' action='/'>";
    html += "<button type='submit'>Back to Main Page</button>";
    html += "</form>";

    html += "</body></html>";

    server.send(200, "text/html", html);
}

extern SDManager sd; // Add this at top of file


String WifiApServer::generateHtmlPage() {
    String html = "<!DOCTYPE html><html><head><title>LundaLogger Status</title></head><body>";

    // 🌈 Set background color to your TFT color (#84B6D6)
    html += "<body style='background-color: #84B6D6; font-family: Arial, sans-serif;'>";

    // --- SD Card Status ---
    sd.updateCardStatus();
    bool sdOk = sd.isCardPresent();

    String sdStatusHtml;

    if (sdOk) {
        uint64_t totalBytes = SD.cardSize();
        uint64_t usedBytes  = SD.usedBytes();

        float totalMb = totalBytes / (1024.0 * 1024.0);
        float usedMb  = usedBytes  / (1024.0 * 1024.0);
        float freeMb = totalMb - usedMb;

        sdStatusHtml += F("<div style='");
        sdStatusHtml += F("background-color: #e0f8e0; color: #207520;");
        sdStatusHtml += F("border: 1px solid #207520;");
        sdStatusHtml += F("padding: 10px; text-align: center; font-weight: normal;");
        sdStatusHtml += F("margin-bottom: 10px;'>");
        sdStatusHtml += "<strong>SD OK</strong> - Size: " + String(totalMb, 1) + 
                        " MB, Used: " + String(usedMb, 1) + 
                        " MB, Free: " + String(freeMb, 1) + " MB";
        sdStatusHtml += "</div>";
    } else {
        sdStatusHtml += F("<div style='");
        sdStatusHtml += F("background-color: #fdd; color: #a00000;");
        sdStatusHtml += F("border: 1px solid #a00000;");
        sdStatusHtml += F("padding: 10px; text-align: center; font-weight: bold;");
        sdStatusHtml += F("margin-bottom: 10px;'>");
        sdStatusHtml += "<strong>SD FAIL</strong>";
        sdStatusHtml += "</div>";
    }

    html += sdStatusHtml;

    // --- Title with logo ---
    html += "<div style='text-align: center; padding: 30px 20px 20px;'>";
    html += "<div style='font-size: 32px; font-weight: bold; color: black; text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.3);'>";
    html += "LundaLogger";
    html += "</div>";
    html += "<div style='background-image: url(\"/logo\"); background-size: contain; background-repeat: no-repeat; background-position: top center; height: 180px;'>";
    html += "</div></div><br>";

    // --- Values List ---
    html += "<ul>";
    for (int i = 0; i < 4; ++i) {
        // html += "<li>" + _labels[i] + ": " + String(_values[i]) + "</li>"; // label and value
        html += "<li>" + _labels[i]; // label onlu
    }
    html += "</ul>";

    // --- Buttons ---
    if (_downloadEnabled || _deleteEnabled) {
        html += "<br><form method='GET' action='/files'>";
        html += "<button type='submit'>Open File Manager</button>";
        html += "</form>";
    }

    // ✅ New "View Config Files" button
    html += "<br><form method='GET' action='/configs'>";
    html += "<button type='submit'>View Config Files</button>";
    html += "</form>";

    html += "</body></html>";
    return html;
}

void WifiApServer::freePsramImage() {
    if (imageBuffer) {
        free(imageBuffer);
        imageBuffer = nullptr;
        imageSize = 0;
        Serial.println("✅ PSRAM image buffer freed.");
    } else {
        Serial.println("ℹ️ No PSRAM image buffer to free.");
    }
}

bool WifiApServer::loadLogoToPsram(const char* path) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.println("❌ Failed to open image file: " + String(path));
        return false;
    }

    imageSize = file.size();
    imageBuffer = (uint8_t*)ps_malloc(imageSize);
    if (!imageBuffer) {
        Serial.println("❌ PSRAM allocation failed for image");
        file.close();
        return false;
    }

    size_t bytesRead = file.read(imageBuffer, imageSize);
    file.close();

    if (bytesRead != imageSize) {
        Serial.println("❌ Incomplete read from SPIFFS");
        free(imageBuffer);
        imageBuffer = nullptr;
        return false;
    }

    if (psramFound()) {
        hostCom.println("✅ PSRAM is available.");
        hostCom.println("Total PSRAM: " + String(ESP.getPsramSize()) + " bytes");
        hostCom.println("Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");
    } else {
        hostCom.println("⚠️ PSRAM not found.");
    }

    Serial.println("✅ Image loaded into PSRAM successfully.");
    return true;
}
