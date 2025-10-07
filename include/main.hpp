#ifndef MAIN_HPP
#define MAIN_HPP

// -----------------------------------------------
//
// main.hpp for the Lundalogger
//
// Åke L 2025-03
//
// -----------------------------------------------



// Colours for LundaLogger
#define TFT_LOGOBACKGROUND       0x85BA // note, if you pick another color from the image, note that you will have to flip the bytes here
#define TFT_LOGOBLUE             0x5497
#define TFT_DARKERBLUE           0x3A97 // A muted steel blue
#define TFT_DEEPBLUE             0x1A6F // A darker steel blue
#define TFT_SLATEBLUE            0x2B4F // A lighter steel blue
#define TFT_MIDNIGHTBLUE         0x1028 // A light steel blue
#define TFT_REDDISH_TINT         0xA4B2   
#define TFT_GREENISH_TINT        0x5DAD

#define lundaLoggerVerLbl "LundaLogger 2025-03-20 1.0.0"
#define VERSION "1.0.0"

#define hostCom Serial
#define servoCom Serial2
#define RXD2 16
#define TXD2 17
#define SERVO_BAUD 38400    // baud rate for ventilator communication

#define LCD_WIDTH  320
#define LCD_HEIGHT 170

#define INTERACTION_BUTTON_PIN 14 // GPIO14, key 2
#define LONG_KEY_PRESS_DURATION 1000 // long press, in [ms]

// #define SDCARD_CS_PIN 5

#define SET_LOOP_TIME 200000      // slow loop update in [us]

// CIE communication timing
const unsigned long TIMEOUT_MS = 20000;      // 20 seconds without data = lost
const unsigned long INIT_INTERVAL_MS = 5000; // Retry INIT every 5 seconds


#include <Arduino.h>

#include "Free_Fonts.h" // Optional helper for shorthand like FSB24

#include "constants.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "Lundalogger_logo_100x100_24bit.h"
#include "ImageRenderer.hpp"
#include <SPI.h>
#include "SDManager.hpp"
#include "WifiApServer.hpp"

#include "ServoCIEData.hpp"
// const char* MetricConfigPath = "/MetricConfig.txt";
// const char* SettingConfigPath = "/SettingConfig.txt";
#include "Button.hpp"

#endif // MAIN_HPP