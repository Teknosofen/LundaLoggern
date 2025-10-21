#include "SDManager.hpp"
#include <SD.h>


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

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    setBusy(false);
    return false;
  }

  file.seek(0);  // Overwrite
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

bool SDManager::updateCardStatus() {
  static unsigned long lastCheckTime = 0;
  const unsigned long checkInterval = 1000;   // ms between checks
  const unsigned long retryDelay    = 3000;   // ms backoff after failure
  unsigned long now = millis();

  // Don’t check too often
  if (now - lastCheckTime < checkInterval) return false;
  lastCheckTime = now;

  if (isBusy()) {
    hostCom.println("🕓 SD busy — skipping status check.");
    return false;
  }

  bool currentlyPresent = false;

  // --- Case 1: Card is *not* mounted right now ---
  if (!cardPresent) {
    // Respect retry backoff
    if (retryCount > 0 && now - lastRetryTime < retryDelay) {
      hostCom.println("⏳ Backing off SD retry...");
      return false;
    }

    hostCom.println("🔄 Attempting to mount SD...");
    if (SD.begin(cs, spi)) {
      // Consider it present only if FS looks valid
      currentlyPresent = (SD.cardType() != CARD_NONE && SD.cardSize() > 0);
    } else {
      hostCom.println("❌ SD.begin() failed");
    }

    lastRetryTime = now;
    retryCount++;
  }

  // --- Case 2: Card *was* mounted, verify still OK ---
  else {
    if (SD.cardType() == CARD_NONE || SD.cardSize() == 0) {
      hostCom.println("⚠️ SD card no longer responding, unmounting.");
      SD.end();
      currentlyPresent = false;
    } else {
      // Optionally verify root access
      File root = SD.open("/");
      if (!root) {
        hostCom.println("⚠️ Cannot access root directory, unmounting.");
        SD.end();
        currentlyPresent = false;
      } else {
        root.close();
        currentlyPresent = true;
        retryCount = 0; // reset retry count
      }
    }
  }

  // --- If card status changed, update state + log ---
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

      // ✅ Now it's safe to list root
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

    file.close();                  // ✅ close before reassigning
    file = root.openNextFile();
  }

  root.close();                    // ✅ close the directory handle
}




bool SDManager::updateFileNameIfChanged(char dataType, String &outFileName) {
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);

    // Determine 12h interval start: set hour to 00 or 12, minute to 00
    int intervalHour = timeinfo->tm_hour < 12 ? 0 : 12;
    // if 6h is needed:
    
    // Determine 6h interval start
    // int intervalHour = (timeinfo->tm_hour / 6) * 6;

    // Create a copy of timeinfo to modify
    struct tm intervalTime = *timeinfo;
    intervalTime.tm_hour = intervalHour;
    intervalTime.tm_min = 0;
    intervalTime.tm_sec = 0;

    // Format timestamp as YYYYMMDD_HHMM
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M", &intervalTime);
    String generatedName;

    // Prefix based on dataType
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

    return (generatedName != currentFileName);
}


void SDManager::appendData(String data, char dataType) {
    String newFileName;
    bool isNewFile = updateFileNameIfChanged(dataType, newFileName);

    if (isNewFile) {
        currentFileName = newFileName;
    }

    File file = SD.open(currentFileName, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        hostCom.println(currentFileName);
        return;
    }

    // if (isNewFile) {
    //     if (dataType == 'M') {
    //         file.println("Timestamp,Temperature,Humidity,Pressure,Battery");
    //     } else if (dataType == 'S') {
    //         file.println("Timestamp,Setting1,Setting2,Setting3"); // placeholder
    //     }
    // }

    file.print(data);
    file.close();
}

void SDManager::deleteAllFiles(const char* path) {
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open root directory");
        return;
    }

    File entry = root.openNextFile();
    while (entry) {
        String name = entry.name();
        if (entry.isDirectory()) {
            Serial.print("Skipping directory: ");
            Serial.println(name);
            // Optionally: recursively delete contents
        } else {
            String fullPath = "/" + name;
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