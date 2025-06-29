#include <Arduino.h>
#include "main.hpp"


ServoCIEData servoCIEData;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(115200); // Assuming Serial2 is used for communication with the ventil
  Serial.println("LundaLogger setup started");

  servoCIEData.begin();
  Serial.println("LundaLogger setup completed");
  // Additional setup code can go here
  // For example, initializing the display, WiFi, or other peripherals
  // TFT_eSPI tft = TFT_eSPI(); // Initialize the display
  // tft.init();
  // tft.setRotation(1); // Set display orientation
  // tft.fillScreen(TFT_BLACK); // Clear the display
  // tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color and background
  // tft.setTextSize(2); // Set text size
  // tft.setCursor(10, 10); // Set cursor position
  // tft.println("LundaLogger Ready"); // Print a message on the display
  // Initialize SD card
  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
  } else {
    Serial.println("SD card initialized successfully.");
  }
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
  // put your main code here, to run repeatedly:
}
