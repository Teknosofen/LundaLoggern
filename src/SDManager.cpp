#include "SDManager.hpp"
#define hostCom Serial // test

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
    hostCom.println("🕓 SD busy — skipping status check.");
    return false;
  }

  bool currentlyPresent = false;

  if (!cardPresent) {
    if (retryCount > 0 && now - lastRetryTime < retryDelay) {
      hostCom.println("⏳ Backing off SD retry...");
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
      hostCom.println("⚠️ SD card may have been removed.");
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
    } else {
      hostCom.println("🔴 SD card removed.");
    }

    return true;
  }

  return false;
}
