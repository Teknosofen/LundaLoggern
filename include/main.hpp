#ifndef MAIN_HPP
#define MAIN_HPP

#include <Arduino.h>
#include "constants.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>





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


#endif // MAIN_HPP