#ifndef MAIN_HPP
#define MAIN_HPP

// =============================================================================
//  main.hpp — project-wide configuration for LundaLoggern
// -----------------------------------------------------------------------------
//  LundaLoggern is a standalone data logger for SERVO-u ventilators, running on
//  a LilyGo T-Display S3 (ESP32-S3). It reads the ventilator's CIE/SCI serial
//  stream, logs metrics, settings and alarms to SD card, and offers the logs
//  over a WiFi web interface and a USB-CDC command line (see HostCli.hpp).
//
//  This header is the single place for:
//    - firmware version strings
//    - pin assignments and serial-port aliases
//    - timing constants for the main loop and the CIE link supervision
//    - WiFi access-point and web-login credentials
//    - the display colour palette
//  It also pulls in every project header, so a .cpp file normally only needs
//  `#include "main.hpp"`.
//
//  Åke L 2025-03
// =============================================================================


// --- Display colour palette (RGB565) -----------------------------------------
// TFT_eSPI uses 16-bit RGB565 colours. TFT_LOGOBACKGROUND (0x85BA) is
// RGB(132,182,214) = #84B6D6, the same blue as the web pages' background.
// Note: if you sample a colour from the logo bitmap, the two bytes of the
// 16-bit value may need to be swapped before use here.
#define TFT_LOGOBACKGROUND       0x85BA // logo / screen background (#84B6D6)
#define TFT_LOGOBLUE             0x5497 // mid blue
#define TFT_DARKERBLUE           0x3A97 // muted steel blue
#define TFT_DEEPBLUE             0x1A6F // dark steel blue: default text colour
#define TFT_SLATEBLUE            0x2B4F // dark slate: breath-phase "pause"
#define TFT_MIDNIGHTBLUE         0x1028 // very dark navy
#define TFT_REDDISH_TINT         0xA4B2 // muted red: breath-phase "expiration"
#define TFT_GREENISH_TINT        0x5DAD // muted green: breath-phase "inspiration"

// --- Version -----------------------------------------------------------------
// When releasing: bump VERSION, and update the date inside lundaLoggerVerLbl
// by hand (the label embeds VERSION automatically).
#define VERSION           "1.3.0"                              // Short: shown on TFT main screen, USB banner
#define lundaLoggerVerLbl "LundaLogger 2026-05-18 " VERSION    // Long: boot screen & web dashboard

// --- Serial ports --------------------------------------------------------------
// hostCom  = USB-CDC port to the PC (debug output and the HostCli interface).
// servoCom = UART2, wired via an RS232 transceiver to the ventilator.
//            Runs at SERVO_BAUD with 8 data bits, even parity, 1 stop bit
//            (SERIAL_8E1, set in setup()). Never connect GPIO directly to RS232
//            voltage levels.
#define hostCom Serial
#define servoCom Serial2
#define RXD2 16             // UART2 RX  <- transceiver receiver output (RO)
#define TXD2 17             // UART2 TX  -> transceiver driver input (DI)
#define SERVO_BAUD 38400    // baud rate for ventilator communication

// --- Display geometry (landscape, rotation 1) --------------------------------
#define LCD_WIDTH  320
#define LCD_HEIGHT 170

// --- User button -------------------------------------------------------------
#define INTERACTION_BUTTON_PIN 14 // GPIO14 ("key 2" on the board); long press = WiFi on, short = WiFi off
#define LONG_KEY_PRESS_DURATION 1000 // long press threshold, in [ms]. Not passed to Button; its default is also 1000

// --- Main loop timing ----------------------------------------------------------
// loop() runs the serial parser, web server and USB CLI on every pass, and does
// slower housekeeping (clock, SD status, link supervision) every SET_LOOP_TIME.
#define SET_LOOP_TIME 200000      // slow-loop period in [us] (= 200 ms)

// --- CIE link supervision ------------------------------------------------------
// If no complete frame has been received for TIMEOUT_MS, the link is declared
// lost; while lost, the full CIE init sequence is retried every INIT_INTERVAL_MS.
const unsigned long TIMEOUT_MS = 8000;      // 8 s without data = link lost
const unsigned long INIT_INTERVAL_MS = 5000; // retry INIT every 5 s

// --- WiFi access point and web login -------------------------------------------
// The AP is off at boot and is started by a long button press.
// The web pages /files, /configs, /download and /delete require HTTP basic
// authentication with LOGGER_WEB_USER / LOGGER_WEB_PW; the dashboard "/" does not.
#define LOGGER_SSID "LundaLoggern"
#define LOGGER_PW "neonatal"
#define LOGGER_WEB_USER "admin"
#define LOGGER_WEB_PW LOGGER_PW

// --- Includes ------------------------------------------------------------------
// Kept below the #defines on purpose: several project headers use the macros
// above (hostCom, VERSION, colours ...).
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
#include <qrcode.h>

#include "ServoCIEData.hpp"
#include "Button.hpp"
#include "HostCli.hpp"

#endif // MAIN_HPP
