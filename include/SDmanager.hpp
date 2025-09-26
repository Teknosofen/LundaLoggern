#pragma once
#include <Arduino.h>
#include "main.hpp"
#include <SPI.h>
#include <SD.h>

extern DateTime dateTime;

class SDManager {
public:
  SDManager(SPIClass &spiBus, uint8_t csPin, DateTime* dateTime);

  bool begin();
  void setCardPresent(bool status);
  bool isCardPresent() const;
  bool updateCardStatus();  // Returns true if status changed

  bool writeTextFile(const char* path, const String& content);
  bool appendTextFile(const char* path, const String& content);
  void listRoot();
  void deleteAllFiles(const char* path  = "/");

  String getCurrentFileName(char dataType);
  void appendData(String data, char dataType);

  enum SDState {
    SD_IDLE,
    SD_BUSY,
    SD_ERROR
  };

    SDState state;
    uint8_t retryCount;
    unsigned long lastRetryTime;


void setBusy(bool active);
bool isBusy() const;


private:
  SPIClass &spi;
  uint8_t cs;
  bool cardPresent;

  DateTime* dateTime;

  String currentFileName = "";
};