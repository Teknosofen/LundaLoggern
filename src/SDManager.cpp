// =============================================================================
//  SDManager.cpp — SD card mount state and log files. See SDManager.hpp.
// =============================================================================

#include "SDManager.hpp"
#include "HostCli.hpp"
#include <SD.h>


// The DateTime pointer is accepted but not stored (the `dateTime` member stays
// unset); file naming reads the system clock directly.
SDManager::SDManager(SPIClass &spiBus, uint8_t csPin, DateTime* dateTime)
  : spi(spiBus), cs(csPin), cardPresent(false), state(SD_IDLE),
    retryCount(0), lastRetryTime(0) {}

bool SDManager::begin() {
  if (SD.begin(cs, spi)) {
    setCardPresent(true);
    return true;
  } else {
    setCardPresent(false);
    return false;
  }
}

void SDManager::setCardPresent(bool status) {
  cardPresent = status;
}

bool SDManager::isCardPresent() const {
  return cardPresent;
}

void SDManager::setBusy(bool active) {
  state = active ? SD_BUSY : SD_IDLE;
}

bool SDManager::isBusy() const {
  return state == SD_BUSY;
}

bool SDManager::writeTextFile(const char* path, const String& content) {
  if (!cardPresent) return false;
  setBusy(true);

  File file = SD.open(path, FILE_WRITE);   // on ESP32, FILE_WRITE creates or truncates
  if (!file) {
    setBusy(false);
    return false;
  }

  file.seek(0);  // already at 0 after truncation; kept for clarity
  file.print(content);
  file.close();
  setBusy(false);
  return true;
}

bool SDManager::appendTextFile(const char* path, const String& content) {
  if (!cardPresent) return false;
  setBusy(true);

  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    setBusy(false);
    return false;
  }

  file.print(content);
  file.close();
  setBusy(false);
  return true;
}

// Hot-plug detection. The SD library has no card-detect interrupt, so this
// polls: when unmounted, try to mount; when mounted, check the card still
// answers. To keep the main loop responsive, checks are limited to one per
// second, with a 3 s back-off after a failed mount.
bool SDManager::updateCardStatus() {
  static unsigned long lastCheckTime = 0;     // shared by all callers (main loop and web page)
  const unsigned long checkInterval = 1000;   // ms between checks
  const unsigned long retryDelay    = 3000;   // ms backoff after failure
  unsigned long now = millis();

  if (now - lastCheckTime < checkInterval) return false;
  lastCheckTime = now;

  if (isBusy()) {
    hostCom.println("🕓 SD busy — skipping status check.");
    return false;
  }

  bool currentlyPresent = false;

  // --- Case 1: card is not mounted: try to mount it ---
  if (!cardPresent) {
    if (retryCount > 0 && now - lastRetryTime < retryDelay) {
      hostCom.println("⏳ Backing off SD retry...");
      return false;
    }

    hostCom.println("🔄 Attempting to mount SD...");
    if (SD.begin(cs, spi)) {
      // Consider it present only if the card reports a type and a size.
      currentlyPresent = (SD.cardType() != CARD_NONE && SD.cardSize() > 0);
    } else {
      hostCom.println("❌ SD.begin() failed");
    }

    lastRetryTime = now;
    retryCount++;
  }

  // --- Case 2: card was mounted: verify it still answers ---
  else {
    if (SD.cardType() == CARD_NONE || SD.cardSize() == 0) {
      hostCom.println("⚠️ SD card no longer responding, unmounting.");
      SD.end();
      currentlyPresent = false;
    } else {
      // cardType()/cardSize() report values cached at mount time, so they
      // don't prove the card is still there; opening the root is an extra check.
      File root = SD.open("/");
      if (!root) {
        hostCom.println("⚠️ Cannot access root directory, unmounting.");
        SD.end();
        currentlyPresent = false;
      } else {
        root.close();
        currentlyPresent = true;
        retryCount = 0;
      }
    }
  }

  // --- On a state change: update, log card details, report the change ---
  if (currentlyPresent != cardPresent) {
    setCardPresent(currentlyPresent);

    if (currentlyPresent) {
      hostCom.println("🟢 SD card just inserted!");
      retryCount = 0;

      uint8_t type = SD.cardType();
      hostCom.print("📦 Type: ");
      switch (type) {
        case CARD_MMC:  hostCom.println("MMC"); break;
        case CARD_SD:   hostCom.println("SD"); break;
        case CARD_SDHC: hostCom.println("SDHC"); break;
        default:        hostCom.println("Unknown"); break;
      }

      hostCom.print("📏 Size: ");
      hostCom.print(SD.cardSize() / (1024 * 1024));
      hostCom.println(" MB");

      hostCom.print("📁 Total: ");
      hostCom.print(SD.totalBytes() / (1024 * 1024));
      hostCom.println(" MB");

      hostCom.print("🧹 Used: ");
      hostCom.print(SD.usedBytes() / (1024 * 1024));
      hostCom.println(" MB");

      listRoot();
    } else {
      hostCom.println("🔴 SD card removed.");
    }

    return true; // status changed
  }

  return false; // no change
}


void SDManager::listRoot() {
  hostCom.println("Listing files on SD card:");

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    hostCom.println("Failed to open SD root");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    hostCom.print("  ");
    hostCom.print(file.name());

    if (file.isDirectory()) {
      hostCom.println(" <DIR>");
    } else {
      hostCom.print("  ");
      hostCom.print(file.size());
      hostCom.println(" bytes");
    }

    file.close();                  // close each entry before opening the next
    file = root.openNextFile();
  }

  root.close();
}


// Build "/<type>_YYYYMMDD_HHMM.txt" for the current 12-hour window, where HHMM
// is 0000 (00:00-11:59) or 1200 (12:00-23:59).
bool SDManager::updateFileNameIfChanged(char dataType, String &outFileName) {
    time_t now = time(nullptr);              // system clock, set from the ventilator (RTIM)
    struct tm *timeinfo = localtime(&now);

    int intervalHour = timeinfo->tm_hour < 12 ? 0 : 12;
    // For 6-hour files instead, use:
    // int intervalHour = (timeinfo->tm_hour / 6) * 6;

    // Start of the window: same date, hour 00 or 12, minute and second 0.
    struct tm intervalTime = *timeinfo;
    intervalTime.tm_hour = intervalHour;
    intervalTime.tm_min = 0;
    intervalTime.tm_sec = 0;

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M", &intervalTime);
    String generatedName;

    if (dataType == 'M') {
        generatedName = "metrics_" + String(buffer) + ".txt";
    } else if (dataType == 'S') {
        generatedName = "settings_" + String(buffer) + ".txt";
    } else if (dataType == 'C') {
        generatedName = "curves_" + String(buffer) + ".txt";
    } else if (dataType == 'A') {
        generatedName = "alarms_" + String(buffer) + ".txt";
    }

    generatedName = "/" + generatedName;
    outFileName = generatedName;

    // Return true when this specific file does not yet exist (or is empty),
    // i.e. when a header should be written. Comparing to a shared
    // currentFileName would falsely report "new" every time the data type
    // alternates between metrics, settings and alarms.
    if (!SD.exists(generatedName.c_str())) {
        return true;
    }
    File probe = SD.open(generatedName, FILE_READ);
    bool empty = false;
    if (probe) {
        empty = (probe.size() == 0);
        probe.close();
    }
    return empty;
}


void SDManager::appendData(String data, char dataType) {
    String newFileName;
    updateFileNameIfChanged(dataType, newFileName);   // only the path is needed here
    currentFileName = newFileName;

    File file = SD.open(currentFileName, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        hostCli.dbg(String("appendData OPEN-FAIL type='") + dataType +
                    "' file=" + currentFileName);
        return;
    }

    size_t written = file.print(data);
    file.close();
    hostCli.dbg(String("appendData type='") + dataType +
                "' file=" + currentFileName +
                " bytes=" + String(written));
}

void SDManager::deleteAllFiles(const char* path) {
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open root directory");
        return;
    }

    File entry = root.openNextFile();
    while (entry) {
        String name = entry.name();   // file name only, without directory
        if (entry.isDirectory()) {
            Serial.print("Skipping directory: ");
            Serial.println(name);
        } else {
            String fullPath = "/" + name;   // assumes path == "/"
            if (SD.remove(fullPath)) {
                Serial.print("Deleted file: ");
                Serial.println(fullPath);
            } else {
                Serial.print("Failed to delete file: ");
                Serial.println(fullPath);
            }
        }
        entry.close();
        entry = root.openNextFile();
    }

    root.close();
}
