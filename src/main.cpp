#include <Arduino.h>
#include "main.hpp"


TFT_eSPI tft = TFT_eSPI(); // Initialize the display
TFT_eSprite spriteLeft = TFT_eSprite(&tft); // Create sprite object left
ImageRenderer renderer(tft);

ServoCIEData servoCIEData;

SPIClass hspi(HSPI);
const SPISettings SENSOR_SPI_SETTINGS = SPISettings(800000, MSBFIRST, SPI_MODE0);
SDManager sd(hspi, HSPI_CS); // Pass your CS pin here

void setup() {

  delay(3000); // Wait for 3 seconds before starting setup
  hostCom.begin(115200); // Initialize Serial for debugging
  // servoCom.begin(115200); // Assuming Serial2 is used for communication with the ventilator
  hostCom.println("LundaLogger setup started");

  servoCIEData.begin();

  hostCom.println("LundaLogger setup completed");
  // Additional setup code can go here
  // For example, initializing the display, WiFi, or other peripherals
 

  
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

  // spriteLeft.createSprite(LCD_WIDTH, LCD_HEIGHT);
  // spriteLeft.setTextColor(TFT_WHITE, TFT_BLACK);
  // spriteLeft.drawString("LundaLoggern", 40, 40, 4);
  // spriteLeft.pushSprite(0, 0);
  // delay(1000); // Delay to allow the display to show the message
  
  renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
  delay(1000); // Delay to allow the display to show the message

  // // Initialize SD card
  // if (!SD.begin(HSPI_CS, hspi, 8000000))  // 8 MHz
  //  {
  //   Serial.println("SD Card initialization failed!");
  // } else {
  //   hostCom.println("SD card initialized successfully.");
  // }

  // Initialize WiFi connection
  // WiFi.begin("your-SSID", "your-PASSWORD");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000); 

  //   Serial.println("Connecting to WiFi...");
  // }  
  // Serial.println("WiFi connected successfully.");
  // Serial.print("IP address: ");  
  // Serial.println(WiFi.localIP());
  // Initialize WebServer or WebSocketsServer if needed
  // WebServer server(80); // Initialize web server on port 80
  // WebSocketsServer webSocket = WebSocketsServer(81); // Initialize WebSocket server
  // server.begin(); // Start the web server
  // webSocket.begin(); // Start the WebSocket server
  // Serial.println("Web server and WebSocket server started.");
  // Additional initialization code can be added here

}

void loop() {
  static bool initLoop = false; // Flag to check if loop has been initialized
  if (!initLoop) {
    initLoop = true; // Set the flag to true to indicate loop has been initialized
    hostCom.println("LundaLogger loop initialized");
    renderer.drawSwatch(10, 10, TFT_DARKERBLUE, "INIT");
    delay(2000); // Delay to allow the display to show the message
    tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display with blue color
    renderer.pushFullImage(220, 40, 100, 100, lundaLogo);
    // renderer.drawSwatch(10, 40, TFT_DEEPBLUE, "DEEPBLUE");
    // renderer.drawSwatch(10, 70, TFT_SLATEBLUE, "SLATEBLUE", true);
    renderer.drawSDStatusIndicator(sd.isCardPresent());
  }
  // put your main code here, to run repeatedly:
  // uint16_t bgColor = lundaLogo[0]; // Top-left pixel'uint16_t imageColor = myImage[0]; // Assume this is 0xBA85 in little-endian
  // tft.fillScreen((bgColor >> 8) | (bgColor << 8)); // Should match image

  tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
  tft.setTextSize(2); // Set text size
  tft.setCursor(10, 10); // Set cursor position
  tft.drawString("LundaLogger Loop", 10, 10, 2); // Print a message on the display
  tft.setCursor(10, 50); // Set cursor position for next line
  tft.drawString("Running...", 10, 50, 2); // Print another message on the display

  // renderer.drawSwatch(10, 10, TFT_DARKERBLUE, "DARKERBLUE");
  // renderer.drawSwatch(10, 40, TFT_DEEPBLUE, "DEEPBLUE");
  // renderer.drawSwatch(10, 70, TFT_SLATEBLUE, "SLATEBLUE", true);
  // renderer.drawSwatch(10, 100, 100, 20,  TFT_MIDNIGHTBLUE, "MIDNIGHTBLUE");

  delay(SET_LOOP_TIME); // Wait for the defined loop time
  hostCom.println("looping..."); // Print a message to the host serial port
  

  if (sd.updateCardStatus()) {
    Serial.println("🔄 SD status changed!");
    // Optional: trigger redraw, disable logging, etc.
    renderer.drawSDStatusIndicator(sd.isCardPresent());
    hostCom.print("SD status updated: ");
    hostCom.println(sd.isCardPresent() ? "true" : "false");
  }
  

  }