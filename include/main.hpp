#ifndef MAIN_HPP
#define MAIN_HPP

#include <Arduino.h>
#include "constants.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include "Lundalogger_logo_100x100_24bit.h"
#include "ImageRenderer.h"





// -----------------------------------------------
//
// main.hpp for the Lundalogger
//
// �ke L 2025-03
//
// -----------------------------------------------

#define lundaLoggerVerLbl "LundaLogger 2025-03-20 1.0.0"

#define hostCom Serial
#define servoCom Serial2

#define LCD_WIDTH  320
#define LCD_HEIGHT 170

#define SDCARD_CS_PIN 5

#define SET_LOOP_TIME 1000                                  // slow loop update in [ms]

#include "ServoCIEData.hpp"


// Colours fro LundaLogger
#define TFT_LOGOBACKGROUND       0x85BA // note, if you pick another color from the image, note that you will have to flip the bytes here
#define TFT_LOGOBLUE             0x5497
#define TFT_DARKERBLUE           0x3A97 // A muted steel blue
#define TFT_DEEPBLUE             0x1A6F // A darker steel blue
#define TFT_SLATEBLUE            0x2B4F // A lighter steel blue
#define TFT_MIDNIGHTBLUE         0x1028 // A light steel blue

#endif // MAIN_HPP