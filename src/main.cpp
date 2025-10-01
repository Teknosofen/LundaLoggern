#include <Arduino.h>
#include "main.hpp"

// ---------------------------------
// logo BMP file as well as factory default config files are stored in SPIFFS
// to send to device run:
// pio run --target uploadfs
//
// to format SPIFFS run:
// pio run --target cleanfs
// ---------------------------------


// in the TFT_eSPI TFT_eSPI_User_setup_Select.H ensure that the 
// line 133 #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>     // For the LilyGo T-Display S3 based ESP32S3 with ST7789 170 x 320 TFT

// In the TFT_eSPI_User_setup.H 
// line 55 #define ST7789_DRIVER      // Full configuration option, define additional parameters below for this display
// line 87 #define TFT_WIDTH  170 // ST7789 170 x 320
// line 92 #define TFT_HEIGHT 320 // ST7789 240 x 320


TFT_eSPI tft = TFT_eSPI(); // Initialize the display
TFT_eSprite spriteLeft = TFT_eSprite(&tft); // Create sprite object left
ImageRenderer renderer(tft);

// SDManager sd(hspi, HSPI_CS, &dateTime); // Pass your CS pin here

const char* MetricConfigPath = "/MetricConfig.txt";
const char* SettingConfigPath = "/SettingConfig.txt";

SPIClass hspi(HSPI);
const SPISettings SENSOR_SPI_SETTINGS = SPISettings(25000000, MSBFIRST, SPI_MODE0); // 25 MHz

SDManager sd(hspi, HSPI_CS, &dateTime); // Pass your CS pin here

ServoCIEData servoCIEData(&sd);

WifiApServer WiFiserver("LundaLoggern", ""); //"neonatal");

DateTime dateTime; // Global DateTime object

Button interactionKey1(INTERACTION_BUTTON_PIN);  // GPIO14 Key 2

void setup() {
  hostCom.begin(115200); // Initialize hostCom for debugging
  delay(5000); // Wait for Serial to initialize
 
  // ensure that the servoCOM used the appripriate parity!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  hostCom.println("LundaLogger setup started");

  renderer.begin(); //initialize display

  // pinMode(RXD2, INPUT);
  // pinMode(TXD2, OUTPUT);
  servoCom.begin(SERVO_BAUD, SERIAL_8E1, RXD2, TXD2); // RX = GPIO16, TX = GPIO17

  delay(100); // Wait for Serial to initialize

  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); // SCK, MISO, MOSI, CS
  bool initSuccess = sd.begin(); // Attempt to initialize SD card and update internal status
  sd.setCardPresent(initSuccess); // Explicit status tracking, done in method already

  // Debug 
  // sd.deleteAllFiles();  // Delete everything on the SD card

  if (sd.isCardPresent()) {
    hostCom.println("✅ SD card is present and mounted.");

    // Write and append to a file
    if (sd.writeTextFile("/log.txt", "HSPI test initialized.")) {
      hostCom.println("✍️ Wrote initial log file.");
    }

    if (sd.appendTextFile("/log.txt", "\nAppended line via SDManager.")) {
      hostCom.println("📎 Appended to log file.");
    }
  } else {
    hostCom.println("❌ SD card is not detected or failed to mount.");
  }

  // Check where this tst is (to be) made and remove redundant uses, wcheck in the WiiAPServer as well
  if (!SPIFFS.begin(false)) {
      hostCom.println("❌ SPIFFS mount failed");
   } else {
      hostCom.println("✅ SPIFFS mounted successfully");
  }
  
  hostCom.println("Initializing CIE configs:");
  servoCIEData.initializeConfigs(MetricConfigPath, SettingConfigPath);
  hostCom.println("Initializing CIE com:");
  servoCIEData.begin();

  // WiFiserver.setLogoData(lundaLogo, 100, 100);
  WiFiserver.setTextAndValues("LundaLogger", 23.5, 67.8, 1013.2, 3.7);
  WiFiserver.setLabel(0, "Temp");
  WiFiserver.setLabel(1, "Humidity");
  WiFiserver.setLabel(2, "Pressure");
  WiFiserver.setLabel(3, "Battery");

  WiFiserver.enableSdFileDownloads(true);
  WiFiserver.enableSdFileDelete(true);

  // WiFiserver.begin();
  // hostCom.printf("Access Point IP: %s\n", WiFiserver.getApIpAddress());
  renderer.drawString("WiFi disabled", 10, 50, 2); // Print another message on the display
    
  sd.listRoot();  // List all files in root

  interactionKey1.begin();
}

void loop() {

  unsigned long now = millis();

  static bool initLoop = false; // Flag to check if loop has been initialized
  if (!initLoop) {
    initLoop = true; // Set the flag to true to indicate loop has been initialized
    hostCom.println("LundaLogger loop start init");
    renderer.clear(); // Clear the display with blue color
    renderer.setTextSize(1); // Set text size for the next line
    renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
    renderer.drawSDStatusIndicator(sd.isCardPresent());
    renderer.drawMainScreen();
    renderer.drawString(WiFiserver.getApIpAddress(), 10, 50, 2); // Print another message on the display
    hostCom.println("LundaLogger loop now initialized");
  } // init loop

  static uint32_t loopStartTime = micros(); // Record the start time of the loop
  if (micros() - loopStartTime > SET_LOOP_TIME) { // time loop
    // Check if enough time has passed since the last loop iteration
    loopStartTime = micros(); // Reset the start time for the next loop

    // renderer.drawMainScreen();
    // renderer.drawString(WiFiserver.getApIpAddress(), 10, 50, 2); // Print another message on the display
    renderer.drawDateTimeAt(dateTime.getRTC(), 10, 160); // Draw current RTC time
    renderer.drawServoID(servoCIEData.getServoID(), 10, 140);
    renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());
    
    // Update breath phase of display
    static uint8_t lastBreathPhase = 0;
    uint8_t currentBreathPhase = servoCIEData.getBreathPhase();
    if (lastBreathPhase != currentBreathPhase) {
      lastBreathPhase = currentBreathPhase;
    renderer.drawBreathPhase(currentBreathPhase, 180, 140);
    };


    if (sd.updateCardStatus()) {
      hostCom.println("🔄 SD status changed!");
      // Optional: trigger redraw, disable logging, etc.
      renderer.drawSDStatusIndicator(sd.isCardPresent());
      hostCom.print("SD status updated: ");
      hostCom.println(sd.isCardPresent() ? "true" : "false");
    }

    // Supervise communication timeout
    if (servoCIEData.isComOpen() && (now - servoCIEData.getLastMessageTime() > TIMEOUT_MS)) {
        hostCom.println("⚠️ Connection lost due to timeout.");
        servoCIEData.setComOpen(false);
        servoCIEData.setLastInitAttempt(now); // Reset init timer
    }

    // Retry INIT if communication is lost
    if (!servoCIEData.isComOpen() && (now - servoCIEData.getLastInitAttempt() > INIT_INTERVAL_MS)) {
      hostCom.println("🔄 (Re)Sending INIT...");
        if (servoCIEData.CIE_comCheck()) {
            hostCom.println("✅ CIE communication re-established.");
            servoCIEData.setComOpen(true);
            servoCIEData.setLastMessageTime(now); // Reset message timer
            servoCIEData.CIE_setup();
        } else {
            hostCom.println("❌ CIE communication re-check failed.");
        }
        servoCIEData.setLastInitAttempt(now);
    }
  }
  // check for serial data from CIE and ventilator
  if (servoCom.available()) {
    char inByte = servoCom.read();                               // get the byte from the ventilator 
    // hostCom.print(inByte, HEX);
    servoCIEData.parseCIEData(inByte);
  }

  WiFiserver.handleClient();  // This keeps the web server alive
  
  interactionKey1.update();

  if (interactionKey1.wasPressed()) {
    hostCom.println("interactionKey1 pressed");
  }

  if (interactionKey1.wasReleased()) {
      if (interactionKey1.wasLongPress()) {
          hostCom.println("Key1 long press (>1s)");
          WiFiserver.begin();
          hostCom.println("Started WiFi");
          hostCom.printf("Access Point IP: %s\n", WiFiserver.getApIpAddress());
          renderer.drawString(WiFiserver.getApIpAddress(), 10, 50, 2); // Print another message on the display

          // Add dispaly update

      } else {
        hostCom.println("interactionKey1 short press");
          
        // Stop WiFi Access Point
        WiFi.softAPdisconnect(true);  // true = erase settings
        WiFi.mode(WIFI_OFF);          // turn off WiFi radio
        hostCom.println("WiFi Access Point stopped");
        renderer.drawString("WiFi disabled", 10, 50, 2); // Print another message on the display

        // Add display update

      }
  }


}