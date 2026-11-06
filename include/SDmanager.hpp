#pragma once
// =============================================================================
//  SDManager.hpp — SD card mount state and log files
// -----------------------------------------------------------------------------
//  Wraps the Arduino SD library on a dedicated SPI bus (HSPI) and provides:
//    - mounting, plus hot-plug detection with retry back-off (updateCardStatus())
//    - the rolling log files written by ServoCIEData:
//          /metrics_YYYYMMDD_HHMM.txt   ('M')
//          /settings_YYYYMMDD_HHMM.txt  ('S')
//          /alarms_YYYYMMDD_HHMM.txt    ('A')
//          /curves_YYYYMMDD_HHMM.txt    ('C', name reserved; curves are not logged)
//      HHMM is the start of the current 12-hour window, 0000 or 1200, taken
//      from the ESP32 system clock (which is set from the ventilator, see
//      DateTime). A new window simply means a new file name.
//    - small text-file helpers (write, append, list, delete)
//
//  All files live in the SD root; there are no subdirectories.
//
//  Note: the file name is spelled SDmanager.hpp but included as
//  "SDManager.hpp". That works on Windows (case-insensitive file system) only.
// =============================================================================
#include <Arduino.h>
#include "main.hpp"
#include <SPI.h>
#include <SD.h>

extern DateTime dateTime;

class SDManager {
public:
  /// @param spiBus   SPI bus the card is on (must be begun before begin()).
  /// @param csPin    chip-select pin.
  /// @param dateTime currently unused; file names use the system clock directly.
  SDManager(SPIClass &spiBus, uint8_t csPin, DateTime* dateTime);

  /// Mount the card. Returns true on success; also updates isCardPresent().
  bool begin();
  void setCardPresent(bool status);
  bool isCardPresent() const;

  /// Poll for card insertion/removal. Cheap to call often: it does real work
  /// at most once per second, and after a failed mount it waits 3 s before
  /// retrying. Returns true only when the present/absent state changed.
  bool updateCardStatus();

  /// Create or overwrite `path` with `content`. False if no card or open fails.
  bool writeTextFile(const char* path, const String& content);
  /// Append `content` to `path` (created if missing). False if no card or open fails.
  bool appendTextFile(const char* path, const String& content);
  /// Print the files in the SD root, with sizes, to hostCom.
  void listRoot();
  /// Delete every file directly in the root; subdirectories are skipped.
  /// Only works correctly for path = "/". Debug helper, not used in normal operation.
  void deleteAllFiles(const char* path  = "/");

  /// Compute the log file path for `dataType` ('M', 'S', 'A' or 'C') in the
  /// current 12-hour window and return it in outFileName.
  /// Returns true if that file does not exist yet or is empty, i.e. when the
  /// caller should write the file header first. (The name is historic: it
  /// does not track whether the window changed.)
  bool updateFileNameIfChanged(char dataType, String &outFileName);

  /// Append `data` to the current log file for `dataType`. Opens and closes
  /// the file on every call, so a card pulled between writes loses at most
  /// the row being written. Does not check isCardPresent(); a failed open is
  /// reported on hostCom and the row is dropped.
  void appendData(String data, char dataType);

  enum SDState {
    SD_IDLE,
    SD_BUSY,   // a writeTextFile()/appendTextFile() is in progress
    SD_ERROR   // unused
  };

  // Public for historical reasons; treat as internal.
    SDState state;
    uint8_t retryCount;          // failed mount attempts since the card was last OK
    unsigned long lastRetryTime; // millis() of the last mount attempt


/// Busy flag: updateCardStatus() skips its check while a text-file write runs.
void setBusy(bool active);
bool isBusy() const;


private:
  SPIClass &spi;
  uint8_t cs;
  bool cardPresent;

  DateTime* dateTime;  // never assigned; see constructor

  String currentFileName = "";  // last file appendData() wrote to
};
