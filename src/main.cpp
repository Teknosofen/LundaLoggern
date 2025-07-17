#include <Arduino.h>
#include "main.hpp"


TFT_eSPI tft = TFT_eSPI(); // Initialize the display
TFT_eSprite spriteLeft = TFT_eSprite(&tft); // Create sprite object left
ImageRenderer renderer(tft);

// ServoCIEData servoCIEData;

// test only
void drawSwatch(int x, int y, uint16_t color, const char* label) {
  tft.fillRect(x, y, 100, 25, color);
  tft.setTextColor(TFT_WHITE, color); // White text on color background
  tft.setCursor(x + 5, y + 7);
  tft.print(label);
}
// End of test only

void setup() {
  // put your setup code here, to run once:
  hostCom.begin(115200); // Initialize Serial for debugging
  // hostCom.begin(115200);
  // servoCom.begin(115200); // Assuming Serial2 is used for communication with the ventilator
  hostCom.println("LundaLogger setup started");

  // servoCIEData.begin();

  hostCom.println("LundaLogger setup completed");
  // Additional setup code can go here
  // For example, initializing the display, WiFi, or other peripherals
 
  tft.init();
  tft.setRotation(1); // Set display orientation
  tft.fillScreen(TFT_RED); // Clear the display
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color and background
  tft.setTextSize(2); // Set text size
  tft.setCursor(10, 10); // Set cursor position
  tft.println("LundaLogger Ready"); // Print a message on the display
  delay(1000); // Delay to allow the display to show the message
  spriteLeft.createSprite(LCD_WIDTH, LCD_HEIGHT);
  spriteLeft.setTextColor(TFT_WHITE, TFT_BLACK);
  spriteLeft.drawString("LundaLoggern", 40, 40, 4);
  spriteLeft.pushSprite(0, 0);
  delay(1000); // Delay to allow the display to show the message
  // renderer.drawImage(0, 100, 100, 100, lundaLogo);
  tft.setCursor(10, 10); // Set cursor position
  tft.println("plupp"); // Print a message on the display
  delay(1000);
  renderer.pushFullImage(100, 0, 100, 100, lundaLogo);


  // Initialize SD card
  // if (!SD.begin(SDCARD_CS_PIN)) {
  //   hostCom.println("SD card initialization failed!");
  // } else {
  //   hostCom.println("SD card initialized successfully.");
  // }
  // Initialize WiFi connection
  // WiFi.begin("your-SSID", "your-PASSWORD");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000); 

  //   Serial.println("Connecting to WiFi...");
  // }  
  Serial.println("WiFi connected successfully.");
  // Serial.print("IP address: ");  
  // Serial.println(WiFi.localIP());
  // Initialize WebServer or WebSocketsServer if needed
  // WebServer server(80); // Initialize web server on port 80
  // WebSocketsServer webSocket = WebSocketsServer(81); // Initialize WebSocket server
  // server.begin(); // Start the web server
  // webSocket.begin(); // Start the WebSocket server
  Serial.println("Web server and WebSocket server started.");
  // Additional initialization code can be added here

}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t bgColor = lundaLogo[0]; // Top-left pixel'uint16_t imageColor = myImage[0]; // Assume this is 0xBA85 in little-endian
  tft.fillScreen((bgColor >> 8) | (bgColor << 8)); // Should match image


  // tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display with blue color

  tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
  tft.setTextSize(2); // Set text size
  tft.setCursor(10, 10); // Set cursor position
  tft.drawString("LundaLogger Loop", 10, 10, 2); // Print a message on the display
  tft.setCursor(10, 50); // Set cursor position for next line
  tft.drawString("Running...", 10, 50, 2); // Print another message on the display


  // spriteLeft.setTextColor(TFT_YELLOW, TFT_BLUE);
  // spriteLeft.drawString("Gurka", 5, 100, 4);
  // spriteLeft.pushSprite(0, 0);
  hostCom.println("LundaLogger loop running");

  renderer.pushFullImage(220, 40, 100, 100, lundaLogo);

    drawSwatch(10, 10, TFT_DARKERBLUE, "DARKERBLUE");
  drawSwatch(10, 40, TFT_DEEPBLUE, "DEEPBLUE");
  drawSwatch(10, 70, TFT_SLATEBLUE, "SLATEBLUE");
  drawSwatch(10, 100, TFT_MIDNIGHTBLUE, "MIDNIGHTBLUE");


  delay(SET_LOOP_TIME); // Wait for the defined loop time
  }

