// =============================================================================
//  main.cpp — LundaLoggern application entry point
// -----------------------------------------------------------------------------
//  Owns all global device objects and ties them together. There is no separate
//  "controller" class: setup() brings the hardware up in order, and loop()
//  services every module without blocking (apart from the CIE re-init, which
//  waits briefly for ventilator replies).
//
//  Data path (see Documentation/DesignSummary.md for the full picture):
//    SERVO-u --RS232--> ServoCIEData (parser) --> SDManager (log files on SD)
//                                             \-> HostCli   (optional USB streams)
//    main.cpp reads parser state (device ID, breath phase, link status) and
//    updates the TFT through ImageRenderer.
//
//  Flashing a new board
//  --------------------
//  The logo bitmap and the factory-default config files live in the SPIFFS
//  partition (folder data/). Upload them once per board, and again whenever a
//  file in data/ changes:
//      pio run --target uploadfs
//  To wipe SPIFFS first:
//      pio run --target cleanfs
//
//  TFT_eSPI library setup (one-time, in .pio/libdeps/.../TFT_eSPI)
//  ---------------------------------------------------------------
//  User_Setup_Select.h must enable only:
//      #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>   // ST7789 170 x 320
//  and User_Setup.h must contain:
//      #define ST7789_DRIVER
//      #define TFT_WIDTH  170
//      #define TFT_HEIGHT 320
// =============================================================================

#include <Arduino.h>
#include "main.hpp"


// --- Global device objects ---------------------------------------------------
// Construction order matters only where one object stores a reference/pointer
// to another; taking the address of a not-yet-constructed global is fine.

TFT_eSPI tft = TFT_eSPI();                   // display driver (pins from TFT_eSPI setup)
TFT_eSprite spriteLeft = TFT_eSprite(&tft);  // currently unused
ImageRenderer renderer(tft);                 // all drawing on the TFT goes through this

// Channel configuration files. Looked up on the SD card first, with the SPIFFS
// copy as fallback (see ServoCIEData::initializeConfigs()).
const char* MetricConfigPath  = "/MetricConfig.txt";
const char* SettingConfigPath = "/SettingConfig.txt";
const char* CurveConfigPath   = "/CurveConfig.txt";
const char* AlarmConfigPath   = "/AlarmConfig.txt";

// The SD card has a dedicated SPI bus (HSPI); pins come from the PlatformIO build flags.
SPIClass hspi(HSPI);
const SPISettings SENSOR_SPI_SETTINGS = SPISettings(25000000, MSBFIRST, SPI_MODE0); // 25 MHz, currently unused

SDManager sd(hspi, HSPI_CS, &dateTime);      // SD card mount state and log files

ServoCIEData servoCIEData(&sd);              // ventilator protocol + channel data; writes logs via `sd`

WifiApServer myWiFiServer(LOGGER_SSID, LOGGER_PW); // SoftAP + web UI, started on demand

DateTime dateTime; // Global DateTime object; the ESP32 system clock is the actual RTC

Button interactionKey1(INTERACTION_BUTTON_PIN);  // GPIO14 ("key 2")


// =============================================================================
// setup() — bring up USB, display, ventilator UART, SD, SPIFFS, configs, CIE
// =============================================================================
void setup() {
  hostCom.begin(115200); // USB-CDC to the PC

  // HostCli prints its READY banner here, so a PC tool can sync on it.
  hostCli.attach(&sd, &servoCIEData);
  hostCli.begin();

  hostCom.println("LundaLogger setup started");

  renderer.begin(); // init TFT and show the boot text

  // The SERVO SCI link requires 8 data bits, EVEN parity, 1 stop bit.
  // With the wrong parity the ventilator silently ignores every command.
  servoCom.begin(SERVO_BAUD, SERIAL_8E1, RXD2, TXD2); // RX = GPIO16, TX = GPIO17

  delay(1000); // give the UART and the transceiver time to settle

  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); // SCK, MISO, MOSI, CS
  bool initSuccess = sd.begin(); // mounts the card; sd.isCardPresent() reflects the result

  // Debug only — DELETES every file in the SD root:
  // sd.deleteAllFiles();

  if (sd.isCardPresent()) {
    hostCom.println("✅ SD card is present and mounted.");

    // Bring-up smoke test: proves write and append work. /log.txt is not part
    // of the logged data and can be deleted at any time.
    if (sd.writeTextFile("/log.txt", "HSPI test initialized.")) {
      hostCom.println("✍️ Wrote initial log file.");
    }

    if (sd.appendTextFile("/log.txt", "\nAppended line via SDManager.")) {
      hostCom.println("📎 Appended to log file.");
    }
  } else {
    hostCom.println("❌ SD card is not detected or failed to mount.");
  }

  // SPIFFS holds the factory-default configs (fallback when the SD card has
  // none) and the logo for the web page. It must be mounted before
  // initializeConfigs(). begin(false) = do not format on failure, so a broken
  // partition is reported instead of being silently wiped.
  if (!SPIFFS.begin(false)) {
      hostCom.println("❌ SPIFFS mount failed");
   } else {
      hostCom.println("✅ SPIFFS mounted successfully");
  }

  // Load channel definitions first: CIE_setup() builds the subscription
  // commands (SDADB/SDADS/SDADA) from them.
  hostCom.println("Initializing CIE configs:");
  servoCIEData.initializeConfigs(MetricConfigPath, SettingConfigPath, CurveConfigPath, AlarmConfigPath);
  hostCom.println("Initializing CIE com:");
  servoCIEData.begin(); // ESC + full init sequence; fails quickly if no ventilator is connected

  // Dashboard content for the web root page. Label slots 0..3 are shown as a list.
  myWiFiServer.setText("LundaLogger");
  myWiFiServer.setLabel(0, lundaLoggerVerLbl);
  myWiFiServer.setLabel(1, " ");
  const String SSIDStr = LOGGER_SSID;
  myWiFiServer.setLabel(2, "SSID: " + SSIDStr);
  myWiFiServer.setLabel(3, servoCIEData.getServoID());

  // Must be set before myWiFiServer.begin(): the /download and /delete routes
  // are only registered if these are enabled when the server starts.
  myWiFiServer.enableSdFileDownloads(true);
  myWiFiServer.enableSdFileDelete(true);

  interactionKey1.begin();

  if (psramFound()) {
    hostCom.println("PSRAM is available.");
    hostCom.println("Total PSRAM: " + String(ESP.getPsramSize()) + " bytes");
    hostCom.println("Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");
  } else {
    hostCom.println("PSRAM not found.");
  }

}

// =============================================================================
// loop() — three layers of work:
//   1. first pass only: draw the static main screen
//   2. every SET_LOOP_TIME (200 ms): clock, breath phase, SD status, link
//      supervision and re-init
//   3. every pass: feed one ventilator byte to the parser, serve HTTP and USB
//      commands, handle the button
// =============================================================================
void loop() {

  unsigned long now = millis(); // timestamp for the link-supervision checks below

  // --- 1. First pass: replace the boot text with the main screen layout ------
  static bool initLoop = false;
  if (!initLoop) {
    initLoop = true;
    hostCom.println("LundaLogger loop start init");
    renderer.clear(); // fill with the logo background colour
    renderer.pushFullImage(-3, -5, 100, 100, lundaLogo);
    renderer.drawStatusField();
    renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());
    renderer.drawSDStatusIndicator(sd.isCardPresent());
    renderer.drawLabel();
    renderer.drawWiFiField();
    renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");  // trailing spaces overwrite longer old text
    renderer.drawWiFiPromt("Press key to enable ");
    renderer.drawServoID(servoCIEData.getServoID());
    renderer.drawDateTimeAt(dateTime.getRTC());

    hostCom.println("LundaLogger now initialized");
  }

  // --- 2. Slow loop (every SET_LOOP_TIME microseconds) -----------------------
  static uint32_t loopStartTime = micros();
  if (micros() - loopStartTime > SET_LOOP_TIME) {
    loopStartTime = micros();

    renderer.drawDateTimeAt(dateTime.getRTC());

    // Redraw the breath-phase dot only when the phase actually changes.
    static uint8_t lastBreathPhase = 0;
    uint8_t currentBreathPhase = servoCIEData.getBreathPhase();
    if (lastBreathPhase != currentBreathPhase) {
      lastBreathPhase = currentBreathPhase;
      renderer.drawBreathPhase(currentBreathPhase);
    };

    // Detect card insertion/removal (rate-limited inside updateCardStatus()).
    if (sd.updateCardStatus()) {
      hostCom.println("🔄 SD status changed!");
      renderer.drawSDStatusIndicator(sd.isCardPresent());
      hostCom.print("SD status updated: ");
      hostCom.println(sd.isCardPresent() ? "true" : "false");
    }

    // Link supervision: the parser refreshes lastMessageTime on every complete
    // frame. Silence for TIMEOUT_MS means the cable is unplugged, the
    // ventilator is in standby, or it stopped streaming.
    if (servoCIEData.isComOpen() && (now - servoCIEData.getLastMessageTime() > TIMEOUT_MS)) {
        hostCom.println("⚠️ Connection lost due to timeout.");
        servoCIEData.setComOpen(false);
        servoCIEData.setLastInitAttempt(now); // first retry after INIT_INTERVAL_MS
        renderer.drawServoID("SERVO:           ");
        renderer.drawCOMStatusIndicator(servoCIEData.isComOpen());
    }

    // While the link is down, probe the ventilator every INIT_INTERVAL_MS and,
    // when it answers, re-run the full init sequence (time, ID, subscriptions,
    // start of continuous streaming). Note: this blocks loop() for the
    // duration of the init (a few seconds when the ventilator answers).
    if (!servoCIEData.isComOpen() && (now - servoCIEData.getLastInitAttempt() > INIT_INTERVAL_MS)) {
      hostCom.println("🔄 (Re)Sending INIT...");
      if (servoCIEData.CIE_comCheck()) {
        hostCom.println("✅ CIE communication re-established.");
        servoCIEData.setComOpen(true);
        servoCIEData.setLastMessageTime(now);
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

      // TODO: when WiFi clients are connected (WiFi.softAPgetStationNum() > 0),
      // show a QR code for http://192.168.4.1/ via QRHandler.
    }
  }

  // --- 3. Every pass ----------------------------------------------------------
  // Feed one byte per pass to the CIE state machine. Bytes that arrive while
  // the loop is busy (SD writes, HTTP requests) wait in the UART receive buffer.
  if (servoCom.available()) {
    char inByte = servoCom.read();
    // hostCom.print(inByte, HEX);              // debug: dump raw ventilator bytes
    servoCIEData.parseCIEData(inByte);
  }

  myWiFiServer.handleClient();                  // serve pending HTTP requests (no-op while WiFi is off)

  hostCli.update();                             // read and execute USB-CDC commands

  // Button: long press (>= LONG_KEY_PRESS_DURATION) starts the WiFi access
  // point and web server, short press stops them. Events are evaluated on
  // release, so the press length is known.
  interactionKey1.update();

  if (interactionKey1.wasReleased()) {
    if (interactionKey1.wasLongPress()) {
      hostCom.println("Key1 long press (>1s)");
      myWiFiServer.begin();
      hostCom.println("Started WiFi");
      hostCom.printf("Access Point IP: %s\n", myWiFiServer.getApIpAddress());
      renderer.drawWiFiAPIP(myWiFiServer.getApIpAddress() + "  ", LOGGER_SSID);
      renderer.drawWiFiPromt("Press key to disable");

    } else {                                    // short press: stop the access point
      hostCom.println("interactionKey1 short press");
      WiFi.softAPdisconnect(true);              // true = erase settings
      WiFi.mode(WIFI_OFF);                      // turn off WiFi radio
      hostCom.println("WiFi Access Point stopped");
      renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");
      renderer.drawWiFiPromt("Press key to enable");
    }
  }
}
