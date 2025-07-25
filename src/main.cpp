#include <Arduino.h>
#include "main.hpp"


TFT_eSPI tft = TFT_eSPI(); // Initialize the display
TFT_eSprite spriteLeft = TFT_eSprite(&tft); // Create sprite object left
ImageRenderer renderer(tft);

ServoCIEData servoCIEData;

SPIClass hspi(HSPI);
const SPISettings SENSOR_SPI_SETTINGS = SPISettings(800000, MSBFIRST, SPI_MODE0);

SDManager sd(hspi, HSPI_CS); // Pass your CS pin here

WifiApServer WiFiserver("LundaLoggern", "neonatal");

void setup() {
  hostCom.begin(115200); // Initialize Serial for debugging
  // servoCom.begin(115200); // Assuming Serial2 is used for communication with the ventilator
  hostCom.println("LundaLogger setup started");

  servoCIEData.begin();
 
  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); // SCK, MISO, MOSI, CS
  // Attempt to initialize SD card and update internal status
  bool initSuccess = sd.begin();
  sd.setCardPresent(initSuccess); // Explicit status tracking

  if (sd.isCardPresent()) {
    Serial.println("✅ SD card is present and mounted.");

    // Write and append to a file
    if (sd.writeTextFile("/log.txt", "HSPI test initialized.")) {
      Serial.println("✍️ Wrote initial log file.");
    }

    if (sd.appendTextFile("/log.txt", "\nAppended line via SDManager.")) {
      Serial.println("📎 Appended to log file.");
    }
  } else {
    Serial.println("❌ SD card is not detected or failed to mount.");
  }


  tft.init();
  tft.setRotation(1); // Set display orientation
  
  tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display
  tft.setTextColor(TFT_WHITE, TFT_LOGOBACKGROUND  ); // Set text color and background
  tft.setTextSize(2); // Set text size
  tft.setCursor(10, 10); // Set cursor position
  tft.println("LundaLogger Ready"); // Print a message on the display
  delay(1000); // Delay to allow the display to show the message

  renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
 
  WiFiserver.setLogoData(lundaLogo, 100, 100);
  WiFiserver.setTextAndValues("LundaLogger", 23.5, 67.8, 1013.2, 3.7);
  WiFiserver.setLabel(0, "Temp");
  WiFiserver.setLabel(1, "Humidity");
  WiFiserver.setLabel(2, "Pressure");
  WiFiserver.setLabel(3, "Battery");

  WiFiserver.enableSdFileDownloads(true);
  WiFiserver.enableSdFileDelete(true);

  WiFiserver.begin();
  hostCom.print("Access Point IP: ");
  hostCom.println(WiFiserver.getApIpAddress());

}

void loop() {

  static bool initLoop = false; // Flag to check if loop has been initialized
  if (!initLoop) {
    initLoop = true; // Set the flag to true to indicate loop has been initialized
    hostCom.println("LundaLogger loop initialized");
    // renderer.drawSwatch(100, 10, TFT_DARKERBLUE, "INIT");
    // delay(500); // Delay to allow the display to show the message
    tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display with blue color
    tft.setTextSize(1); // Set text size for the next line
    renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
    renderer.drawSDStatusIndicator(sd.isCardPresent());
  }

  static uint32_t loopStartTime = millis(); // Record the start time of the loop
  if (micros() - loopStartTime > SET_LOOP_TIME) { // time loop
    // Check if enough time has passed since the last loop iteration
    loopStartTime = micros(); // Reset the start time for the next loop

    /// screen gfx stuff
    tft.setFreeFont(FSSB24);    
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
    // tft.setTextSize(2); // Set text size
    tft.setCursor(10, 10); // Set cursor position
    tft.drawString("LundaLogger", 10, 10); // Print a message on the display
    tft.setCursor(10, 50); // Set cursor position for next line
    tft.setTextSize(1); // Set text size for the next line
    tft.drawString(WiFiserver.getApIpAddress(), 10, 50, 2); // Print another message on the display

    if (sd.updateCardStatus()) {
      hostCom.println("🔄 SD status changed!");
      // Optional: trigger redraw, disable logging, etc.
      renderer.drawSDStatusIndicator(sd.isCardPresent());
      hostCom.print("SD status updated: ");
      hostCom.println(sd.isCardPresent() ? "true" : "false");
    }
  
    // renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
    hostCom.println("looping..."); // Print a message to the host serial port

  } else {
  }

  WiFiserver.handleClient();  // This keeps the web server alive

  }