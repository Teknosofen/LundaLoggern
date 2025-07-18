#include "SDManager.hpp"

SDManager::SDManager(SPIClass &spiBus, uint8_t csPin)
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
  const unsigned long checkInterval = 1000;     // ms between checks
  const unsigned long retryDelay    = 3000;     // ms backoff after failure
  unsigned long now = millis();

  if (now - lastCheckTime < checkInterval) return false;
  lastCheckTime = now;

  if (isBusy()) {
    Serial.println("🕓 SD busy — skipping status check.");
    return false;
  }

  bool currentlyPresent = false;

  if (!cardPresent) {
    if (retryCount > 0 && now - lastRetryTime < retryDelay) {
      Serial.println("⏳ Backing off SD retry...");
      return false;
    }

    SD.end();               // Flush resources
    delay(10);
    currentlyPresent = SD.begin(cs, spi);  // Attempt remount
    lastRetryTime = now;
    retryCount++;
  } else {
    // Verify card is still functional — detect silent removal
    if (SD.cardType() == CARD_NONE || SD.cardSize() == 0 || !SD.open("/")) {
      Serial.println("⚠️ SD card may have been removed.");
      SD.end();
      currentlyPresent = false;
    } else {
      currentlyPresent = true;
      retryCount = 0; // Reset retries since card is present
    }
  }

  if (currentlyPresent != cardPresent) {
    setCardPresent(currentlyPresent);

    if (currentlyPresent) {
      Serial.println("🟢 SD card just inserted!");
      retryCount = 0;

      uint8_t type = SD.cardType();
      Serial.print("📦 Type: ");
      switch (type) {
        case CARD_MMC:  Serial.println("MMC"); break;
        case CARD_SD:   Serial.println("SD"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("Unknown"); break;
      }

      Serial.print("📏 Size: ");
      Serial.print(SD.cardSize() / (1024 * 1024));
      Serial.println(" MB");

      Serial.print("📁 Total: ");
      Serial.print(SD.totalBytes() / (1024 * 1024));
      Serial.println(" MB");

      Serial.print("🧹 Used: ");
      Serial.print(SD.usedBytes() / (1024 * 1024));
      Serial.println(" MB");
    } else {
      Serial.println("🔴 SD card removed.");
    }

    return true;
  }

  return false;
}
// bool SDManager::updateCardStatus() {
//   static unsigned long lastCheckTime = 0;
//   const unsigned long checkInterval = 1000;     // Regular check interval
//   const unsigned long retryDelay    = 3000;     // Backoff delay after failed mount
//   unsigned long now = millis();

//   if (now - lastCheckTime < checkInterval) return false;
//   lastCheckTime = now;

//   if (isBusy()) {
//     Serial.println("🕓 SD busy — skipping status check.");
//     return false;
//   }

//   bool currentlyPresent;

//   if (!cardPresent) {
//     if (retryCount > 0 && now - lastRetryTime < retryDelay) {
//       Serial.println("⏳ Backing off SD retry...");
//       return false;
//     }

//     SD.end();
//     delay(10);
//     currentlyPresent = SD.begin(cs, spi);
//     lastRetryTime = millis();
//     retryCount++;
//   } else {
//     currentlyPresent = (SD.cardType() != CARD_NONE);
//     retryCount = 0;
//   }

//   if (currentlyPresent != cardPresent) {
//     setCardPresent(currentlyPresent);

//     if (currentlyPresent) {
//       Serial.println("🟢 SD card just inserted!");
//       retryCount = 0;

//       uint8_t type = SD.cardType();
//       Serial.print("📦 Type: ");
//       switch (type) {
//         case CARD_MMC: Serial.println("MMC"); break;
//         case CARD_SD: Serial.println("SD"); break;
//         case CARD_SDHC: Serial.println("SDHC"); break;
//         default: Serial.println("Unknown"); break;
//       }

//       Serial.print("📏 Size: ");
//       Serial.print(SD.cardSize() / (1024 * 1024));
//       Serial.println(" MB");

//       Serial.print("📁 Total: ");
//       Serial.print(SD.totalBytes() / (1024 * 1024));
//       Serial.println(" MB");

//       Serial.print("🧹 Used: ");
//       Serial.print(SD.usedBytes() / (1024 * 1024));
//       Serial.println(" MB");
//     } else {
//       Serial.println("🔴 SD card removed.");
//     }

//     return true;
//   }

//   return false;
// }



// #include "SDManager.hpp"

// SDManager::SDManager(SPIClass &spiBus, uint8_t csPin)
//   : spi(spiBus), cs(csPin), cardPresent(false) {}

// bool SDManager::begin() {
//   if (SD.begin(cs, spi)) {
//     cardPresent = true;
//     return true;
//   } else {
//     cardPresent = false;
//     return false;
//   }
// }

// void SDManager::setCardPresent(bool status) {
//   cardPresent = status;
// }

// bool SDManager::isCardPresent() const {
//   return cardPresent;
// }

// bool SDManager::writeTextFile(const char* path, const String& content) {
//   File file = SD.open(path, FILE_WRITE);
//   if (!file) return false;

//   file.seek(0);  // Overwrite
//   file.print(content);
//   file.close();
//   return true;
// }

// bool SDManager::appendTextFile(const char* path, const String& content) {
//   File file = SD.open(path, FILE_APPEND);
//   if (!file) return false;

//   file.print(content);
//   file.close();
//   return true;
// }

// bool SDManager::updateCardStatus() {
//   static unsigned long lastCheckTime = 0;
//   const unsigned long checkInterval = 500; // ms

//   if (millis() - lastCheckTime < checkInterval) {
//     return false; // Too soon to check again
//   }
//   lastCheckTime = millis();

//   bool currentlyPresent;

//   if (!cardPresent) {
//     // Try remounting if we think card is absent
//     currentlyPresent = SD.begin(cs, spi);
//   } else {
//     // If card was present, verify it's still accessible
//     currentlyPresent = (SD.cardType() != CARD_NONE);
//   }

//   if (currentlyPresent != cardPresent) {
//     setCardPresent(currentlyPresent);

//     if (currentlyPresent) {
//       Serial.println("🟢 SD card just inserted!");

//       uint8_t type = SD.cardType();
//       Serial.print("📦 Type: ");
//       switch (type) {
//         case CARD_MMC: Serial.println("MMC"); break;
//         case CARD_SD: Serial.println("SD"); break;
//         case CARD_SDHC: Serial.println("SDHC"); break;
//         default: Serial.println("Unknown"); break;
//       }

//       Serial.print("📏 Size: ");
//       Serial.print(SD.cardSize() / (1024 * 1024));
//       Serial.println(" MB");

//       Serial.print("📁 Total: ");
//       Serial.print(SD.totalBytes() / (1024 * 1024));
//       Serial.println(" MB");

//       Serial.print("🧹 Used: ");
//       Serial.print(SD.usedBytes() / (1024 * 1024));
//       Serial.println(" MB");
//     } else {
//       Serial.println("🔴 SD card was removed.");
//     }

//     return true;
//   }

//   return false;
// }
// // bool SDManager::updateCardStatus() {
// //   bool currentlyPresent = (SD.cardType() != CARD_NONE);

// //   // Check for edge transition
// //   if (currentlyPresent != cardPresent) {
// //     setCardPresent(currentlyPresent);  // Update internal flag

// //     if (currentlyPresent) {
// //       Serial.println("🟢 SD card just inserted!");
      
// //       uint8_t type = SD.cardType();
// //       Serial.print("📦 Card Type: ");
// //       switch (type) {
// //         case CARD_MMC:   Serial.println("MMC"); break;
// //         case CARD_SD:    Serial.println("SD"); break;
// //         case CARD_SDHC:  Serial.println("SDHC"); break;
// //         default:         Serial.println("Unknown"); break;
// //       }

// //       Serial.print("📏 Size: ");
// //       Serial.print(SD.cardSize() / (1024 * 1024));
// //       Serial.println(" MB");

// //       Serial.print("📁 Total: ");
// //       Serial.print(SD.totalBytes() / (1024 * 1024));
// //       Serial.println(" MB");

// //       Serial.print("🧹 Used: ");
// //       Serial.print(SD.usedBytes() / (1024 * 1024));
// //       Serial.println(" MB");
// //     } else {
// //       Serial.println("🔴 SD card was removed.");
// //     }

// //     return true;  // Transition occurred
// //   }

// //   return false;  // No change
// // }