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

const char* MetricConfigPath  = "/MetricConfig.txt";
const char* SettingConfigPath = "/SettingConfig.txt";
const char* CurveConfigPath   = "/CurveConfig.txt";

SPIClass hspi(HSPI);
const SPISettings SENSOR_SPI_SETTINGS = SPISettings(25000000, MSBFIRST, SPI_MODE0); // 25 MHz

SDManager sd(hspi, HSPI_CS, &dateTime); // Pass your CS pin here

ServoCIEData servoCIEData(&sd);

WifiApServer myWiFiServer(LOGGER_SSID, LOGGER_PW); //"neonatal");

DateTime dateTime; // Global DateTime object

Button interactionKey1(INTERACTION_BUTTON_PIN);  // GPIO14 Key 2

// image handling 
// uint8_t* imageBuffer = nullptr;
// size_t imageSize = 0;



void setup() {
  hostCom.begin(115200); // Initialize hostCom for debugging
  // delay(5000); // Wait for Serial to initialize
 
  // ensure that the servoCOM used the appripriate parity!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  hostCom.println("LundaLogger setup started");

  renderer.begin(); //initialize display

  // pinMode(RXD2, INPUT);
  // pinMode(TXD2, OUTPUT);
  servoCom.begin(SERVO_BAUD, SERIAL_8E1, RXD2, TXD2); // RX = GPIO16, TX = GPIO17

  delay(1000); // Wait for Serial to initialize

  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); // SCK, MISO, MOSI, CS
  bool initSuccess = sd.begin(); // Attempt to initialize SD card and update internal status
  // sd.setCardPresent(initSuccess); // Explicit status tracking, done in method already

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
  servoCIEData.initializeConfigs(MetricConfigPath, SettingConfigPath, CurveConfigPath);
  hostCom.println("Initializing CIE com:");
  servoCIEData.begin();
  // myWiFiServer.setText(servoCIEData.getServoID());
  // renderer.drawServoID(servoCIEData.getServoID());

  // myWiFiserver.setLogoData(lundaLogo, 100, 100);
  myWiFiServer.setText("LundaLogger");
  // myWiFiServer.setTextAndValues("LundaLogger", 23.5, 67.8, 1013.2, 3.7);
  myWiFiServer.setLabel(0, lundaLoggerVerLbl);
  myWiFiServer.setLabel(1, " ");
  const String SSIDStr = LOGGER_SSID;
  myWiFiServer.setLabel(2, "SSID: " + SSIDStr);
  myWiFiServer.setLabel(3, servoCIEData.getServoID());

  myWiFiServer.enableSdFileDownloads(true);
  myWiFiServer.enableSdFileDelete(true);
  
  // skipped file listing 
  // sd.listRoot();  // List all files in root

  interactionKey1.begin();

  // image transfer fr SPIFFS to PSRAM
  // File file = SPIFFS.open("/LogoWeb.png", "r");
  // if (!file) {
  //   Serial.println("Failed to open image");
  //   return;
  // }

  // imageSize = file.size();
  // imageBuffer = (uint8_t*)ps_malloc(imageSize);
  // if (!imageBuffer) {
  //   Serial.println("PSRAM allocation failed");
  //   return;
  // }

  // file.read(imageBuffer, imageSize);
  // file.close();


  if (psramFound()) {
    hostCom.println("PSRAM is available.");
    hostCom.println("Total PSRAM: " + String(ESP.getPsramSize()) + " bytes");
    hostCom.println("Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");
  } else {
    hostCom.println("PSRAM not found.");
  }

}

void loop() {

  unsigned long now = millis();

  static bool initLoop = false; // Flag to check if loop has been initialized
  if (!initLoop) {
    initLoop = true; // Set the flag to true to indicate loop has been initialized
    hostCom.println("LundaLogger loop start init");
    renderer.clear(); // Clear the display with blue color
    renderer.pushFullImage(-3, -5, 100, 100, lundaLogo);
    renderer.drawStatusField();
    renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());  
    renderer.drawSDStatusIndicator(sd.isCardPresent());
    renderer.drawLabel();
    renderer.drawWiFiField();
    renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");
    renderer.drawWiFiPromt("Press key to enable ");
    renderer.drawServoID(servoCIEData.getServoID());
    renderer.drawDateTimeAt(dateTime.getRTC()); // Draw current RTC time

    hostCom.println("LundaLogger now initialized");
  } // init loop

  static uint32_t loopStartTime = micros(); // Record the start time of the loop
  if (micros() - loopStartTime > SET_LOOP_TIME) { // time loop, Check if enough time has passed since the last loop iteration
    loopStartTime = micros(); // Reset the start time for the next loop

    renderer.drawDateTimeAt(dateTime.getRTC()); // Draw current RTC time
    
    // Update breath phase of display
    static uint8_t lastBreathPhase = 0;
    uint8_t currentBreathPhase = servoCIEData.getBreathPhase();
    if (lastBreathPhase != currentBreathPhase) {
      lastBreathPhase = currentBreathPhase;
      renderer.drawBreathPhase(currentBreathPhase);
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
        renderer.drawServoID("SERVO:           ");
        renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());
    }

    // Retry INIT if communication is lost
    if (!servoCIEData.isComOpen() && (now - servoCIEData.getLastInitAttempt() > INIT_INTERVAL_MS)) {
      hostCom.println("🔄 (Re)Sending INIT...");
      if (servoCIEData.CIE_comCheck()) {
        hostCom.println("✅ CIE communication re-established.");
        servoCIEData.setComOpen(true);
        servoCIEData.setLastMessageTime(now); // Reset message timer
        servoCIEData.CIE_setup();
        myWiFiServer.setText(servoCIEData.getServoID());
        const String SSIDStr = LOGGER_SSID;
        myWiFiServer.setLabel(2, "SSID: " + SSIDStr);
        myWiFiServer.setLabel(3, servoCIEData.getServoID());
        renderer.drawServoID(servoCIEData.getServoID());
        renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());
      } else {
        hostCom.println("❌ CIE communication re-check failed.");
      }
      servoCIEData.setLastInitAttempt(now);

      // Add check for num connected clients here, if there are client connected, present the second QR code and info
      // add check for num clients in WiFi class:
      // assume that WiFi iff will return zero clients
      // int numClients = WiFi.softAPgetStationNum();
      //     if (numClients > 0 && !qr.isShowingHomepageQR()) {
      // qr.showHomepageQR("http://192.168.4.1/");
      // 

    }
  }
  // check for and manage serial data from CIE and ventilator
  if (servoCom.available()) {
    char inByte = servoCom.read();              // get the byte from the ventilator 
    // hostCom.print(inByte, HEX);
    servoCIEData.parseCIEData(inByte);
  }

  myWiFiServer.handleClient();                  // This keeps the web server alive
  
  interactionKey1.update();
  // if (interactionKey1.wasPressed()) {
  //   hostCom.println("interactionKey1 pressed");
  // }

  if (interactionKey1.wasReleased()) {
    if (interactionKey1.wasLongPress()) {
      hostCom.println("Key1 long press (>1s)");
      myWiFiServer.begin();
      hostCom.println("Started WiFi");
      hostCom.printf("Access Point IP: %s\n", myWiFiServer.getApIpAddress());
      renderer.drawWiFiAPIP(myWiFiServer.getApIpAddress() + "  ", LOGGER_SSID);
      renderer.drawWiFiPromt("Press key to disable");
      // Add QR stuff here

    } else {                                    // Stop WiFi Access Point
      hostCom.println("interactionKey1 short press");
      WiFi.softAPdisconnect(true);              // true = erase settings
      WiFi.mode(WIFI_OFF);                      // turn off WiFi radio
      hostCom.println("WiFi Access Point stopped");
      renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");
      renderer.drawWiFiPromt("Press key to enable");
    }
  }
}