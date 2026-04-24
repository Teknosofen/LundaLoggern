# LundaLoggern — Design Summary

**Version:** 1.2.0 (2026-04-24)  
**Author:** Åke L  
**Platform:** LilyGo T-Display S3 (ESP32-S3)  
**Framework:** Arduino (PlatformIO)

---

## 1. Purpose

LundaLoggern is a portable, standalone data logger designed for **SERVO-u intensive care ventilators**. It continuously captures real-time ventilator metrics, settings, waveform curve data, and **alarm events** via the ventilator's CIE (Computer Interface Equipment) serial interface and stores them as timestamped log files on an SD card. A built-in WiFi access point and web server allow clinical/engineering staff to download, review, and manage logged data wirelessly from any device with a browser.

---

## 2. Hardware Overview

| Component | Details |
|---|---|
| **Microcontroller** | ESP32-S3 (LilyGo T-Display S3) |
| **Display** | 170 × 320 px ST7789 TFT (integrated) |
| **Storage** | MicroSD card via HSPI bus + on-chip SPIFFS |
| **Serial Interface** | RS232 transceiver on UART2 (38 400 baud, 8E1) |
| **User Input** | Physical button on GPIO 14 (short/long press) |
| **Memory** | PSRAM enabled for image buffering |
| **WiFi** | ESP32 SoftAP mode (802.11 b/g/n) |

### Pin Assignments

| Signal | GPIO |
|---|---|
| HSPI MISO | 12 |
| HSPI MOSI | 11 |
| HSPI SCLK | 13 |
| HSPI CS (SD) | 10 |
| UART2 RX (ventilator) | 16 |
| UART2 TX (ventilator) | 17 |
| Interaction Button | 14 |

---

## 3. Software Architecture

### 3.1 Module Overview

```
┌─────────────────────────────────────────────────────────┐
│                      main.cpp                           │
│         (setup / loop orchestration)                    │
├───────────┬───────────┬───────────┬─────────────────────┤
│ ServoCIE  │ SDManager │ WifiAp    │ ImageRenderer       │
│ Data      │           │ Server    │                     │
├───────────┼───────────┼───────────┼─────────────────────┤
│ DateTime  │ Button    │ QRHandler │ Free_Fonts /        │
│           │           │           │ Logo assets         │
└───────────┴───────────┴───────────┴─────────────────────┘
```

### 3.2 Key Classes

| Class | Responsibility |
|---|---|
| **`ServoCIEData`** | CIE protocol parser and command engine. Manages metric, setting, curve, and alarm configurations. Handles serial communication with the ventilator, CRC calculation, data scaling, alarm state tracking, and log file writing via `SDManager`. |
| **`WifiApServer`** | Creates a WiFi SoftAP and HTTP web server. Serves an HTML dashboard for file downloads, file deletion, and configuration review. Displays device logo and status labels. |
| **`SDManager`** | Abstracts all SD card operations (init, read, write, append, delete, file listing). Monitors card presence and manages SPI bus access. Generates date-stamped file names via `DateTime`. |
| **`ImageRenderer`** | Drives the TFT display via TFT_eSPI. Renders the main screen layout including logo, status indicators (SD, COM), WiFi info, ventilator ID, date/time, and breath-phase indicator. |
| **`DateTime`** | Date/time value object with RTC integration. Provides formatted strings for timestamps and file naming. Parses time from ventilator RTIM responses. |
| **`Button`** | Interrupt-driven button handler with debounce, short-press, and long-press detection. |
| **`QRHandler`** | Generates and displays QR codes on the TFT for WiFi credentials and web server URLs. |

### 3.3 Data Flow

```
SERVO-u Ventilator
        │
        │  RS232 / CIE protocol (38400 baud, 8E1)
        ▼
  ┌──────────────┐    parse & scale     ┌────────────┐
  │ ServoCIEData │ ──────────────────►  │ SDManager  │
  │  (parser)    │                      │ (SD card)  │
  └──────┬───────┘                      └────────────┘
         │                                    │
         │ status updates                     │ file listing
         ▼                                    ▼
  ┌──────────────┐                     ┌──────────────┐
  │ImageRenderer │                     │ WifiApServer │
  │  (TFT)       │                     │ (HTTP)       │
  └──────────────┘                     └──────────────┘
```

1. **Ventilator → ServoCIEData**: Raw bytes arrive on UART2 and are parsed character-by-character through a state machine (`RunModeType`). States include `Breath_Data`, `Value_Data`, `Phase_Data`, `Settings_Data`, `Trend_Data`, `Alarm_Data`, and `Error_Data`.
2. **ServoCIEData → SDManager**: Parsed and scaled values (metrics, settings) and alarm priority/status pairs are formatted into tab-separated text lines and appended to date-stamped log files on the SD card (`metrics_*.txt`, `settings_*.txt`, `alarms_*.txt`).
3. **ServoCIEData → ImageRenderer**: Device identity (type + serial number) and breath-phase state are pushed to the display.
4. **SDManager → WifiApServer**: The web server lists SD files and serves them for download or deletion.
5. **DateTime**: Ventilator-derived time (via `RTIM` command) seeds the internal RTC, which timestamps every logged data point and names log files.

---

## 4. Configuration System

Configuration is stored as tab-separated text files, loaded from the SD card (with SPIFFS as fallback/sync). Four configuration types exist:

### MetricConfig.txt — Breath-by-breath measured values
| Column | Meaning | Example |
|---|---|---|
| Channel | CIE channel number | `113` |
| Label | Human-readable name | `VtCO2` |
| Unit | Engineering unit | `ml` |
| Scale Factor | Multiply raw value | `0.1` |
| Offset | Add after scaling | `0.0` |

### SettingConfig.txt — Ventilator settings
Same column format as metrics, using channel numbers in the 400-range (e.g., RR, MV, PEEP, FiO2, mode).

### CurveConfig.txt — High-frequency waveform channels
Adds an `active` flag (true/false) to enable/disable individual curve channels (Flow, Pressure, Volume, Edi, FCO2).

### AlarmConfig.txt — Ventilator alarm channels
Defines which SCI alarm channels (800–999) to subscribe to and log. Same tab-separated format as metrics/settings.

| Column | Meaning | Example |
|---|---|---|
| Channel | CIE alarm channel number (800–999) | `805` |
| Label | Human-readable alarm name | `Apnea` |
| Unit | Type identifier (always `AD`) | `AD` |
| Scale Factor | Not used for alarms (set to `1.0`) | `1.0` |
| Offset | Not used for alarms (set to `0.0`) | `0.0` |
| Active | Enable/disable this alarm channel | `true` |

Alarm data from SCI is transmitted as **priority/value byte pairs** (not gain/offset-scaled values). Each alarm has:
- **Priority**: `0` = undefined, `1` = low, `2` = medium, `3` = high
- **Status**: `0` = no alarm, `1` = alarm active, `2` = alarm active but silenced
- **Not applicable**: The special value `0x7EFF` indicates the alarm channel does not apply in the current ventilation mode.

Logged format is `prio:status` per channel (e.g., `3:1` = high priority, active; `0:0` = no alarm; `N/A` = not applicable).

Default channels include O2 concentration, EtCO2, airway pressure, apnea, gas supply, battery, disconnect, leakage, respiratory rate, minute volume, tidal volume alarms, and summary alarm channels (995–999).

### DeviceConfig.txt — Device-level settings
A key-value configuration file for parameters that govern the device itself (as opposed to ventilator data channels). Lines starting with `#` are comments.

| Key | Default | Description |
|---|---|---|
| `SSID` | `LundaLoggern` | WiFi access-point name |
| `Password` | `neonatal` | WiFi access-point password |
| `WiFiAutoOff_min` | `5` | Minutes of no connected clients before WiFi is automatically disabled. Set to `0` to disable auto-shutoff. |

### Configuration loading priority

**SD card → SPIFFS**. Bi-directional sync between SD and SPIFFS ensures resilience. If a configuration file is not found on the SD card (e.g., a fresh/empty card is inserted), the last known valid copy stored in SPIFFS is written to the SD card as a template. The operator can then edit it on the SD card and the changes will be picked up on next boot.

---

## 5. Communication Protocol

The device communicates with SERVO ventilators using the **CIE (Computer Interface Equipment)** binary/ASCII protocol:

- **Physical layer**: RS232, 38 400 baud, 8 data bits, even parity, 1 stop bit (8E1)
- **Initialization**: Sends `RTIM` (request time), `RCTY` (request device type), `RSEN` (request serial number) commands to identify the ventilator.
- **Data subscription**: Sends `SDADB` / `SDADS` / `SDADA` / `SDADC` commands to subscribe to breath data, settings, alarm, and curve channels respectively.
- **Framing**: Uses `EOT` (0x04) and `ESC` (0x1B) as delimiters; CRC-8 for integrity.
- **Timeout handling**: If no data is received for 8 seconds, the connection is marked lost and re-initialization is attempted every 5 seconds.

---

## 6. WiFi & Web Interface

- **SSID**: `LundaLoggern` (password: `neonatal`)
- **IP**: Default SoftAP IP (`192.168.4.1`)
- **Default state**: WiFi and the web server are **off by default**. The operator must manually start them with a long-press of the physical button. A short-press shuts them down again. This minimises power consumption and RF interference in sensitive clinical environments.
- **Web pages**:
  - **Home / Dashboard**: Displays version, device ID, status labels, and logo.
  - **File Manager**: Lists all SD card log files with download and delete actions.
  - **Config Viewer**: Shows currently loaded metric, setting, curve, and alarm configurations.
- **QR Code**: Display can show a QR code for quick WiFi connection from a phone.

---

## 7. Display Layout

The 320 × 170 TFT is divided into functional zones:

| Zone | Content |
|---|---|
| Top-left | LundaLoggern logo (100 × 100 px) |
| Top-right | Version label |
| Center-right | Connected ventilator type + serial number |
| Bottom-right | Date & time (from ventilator RTC) |
| Right status bar | SD card indicator, COM link indicator, breath-phase indicator |
| Bottom band | WiFi status (AP IP, SSID, prompt to enable/disable) |

---

## 8. Build & Dependencies

**PlatformIO environment**: `lilygo-t-display-s3`

| Library | Version | Purpose |
|---|---|---|
| `bodmer/TFT_eSPI` | 2.5.43 | TFT display driver |
| `ricmoo/QRCode` | ^0.0.1 | QR code generation |
| Arduino core | — | WiFi, SPI, SD, SPIFFS |

**Build flags**: USB CDC on boot, HSPI pin definitions, display variant selection.

### 8.1 Preparing a New Board (First-Time Setup)

When programming a brand-new LilyGo T-Display S3, three separate upload steps are needed:

#### Step 1 — Upload the SPIFFS filesystem image

The `data/` folder contains factory-default configuration files and image assets that must be flashed to the on-chip SPIFFS partition. This only needs to be done **once per new board** (or after a full flash erase), and again whenever files in `data/` are changed.

**From the PlatformIO CLI** (VS Code terminal):

```
pio run --target uploadfs
```

**From the VS Code GUI**: Open the PlatformIO sidebar → Project Tasks → *lilygo-t-display-s3* → Platform → **Upload Filesystem Image**.

This uploads the following files to SPIFFS:

| File | Purpose |
|---|---|
| `MetricConfig.txt` | Breath-by-breath metric channel definitions |
| `SettingConfig.txt` | Ventilator setting channel definitions |
| `CurveConfig.txt` | Waveform curve channel definitions |
| `AlarmConfig.txt` | Alarm channel subscriptions (800–999) |
| `DeviceConfig.txt` | WiFi SSID, password, and auto-off timeout |
| `LogoWeb.png` | Logo served by the web interface |
| `logo150.bmp` | Logo displayed on TFT (loaded to PSRAM) |

> **Note:** SPIFFS does not support subdirectories. All files are placed in the root `/` of the SPIFFS partition.

If SPIFFS needs to be erased/reformatted (e.g. corruption or partition layout change):

```
pio run --target cleanfs
```

Then re-upload the filesystem image with `uploadfs`.

#### Step 2 — Upload the firmware

```
pio run --target upload
```

Or use PlatformIO sidebar → **Upload**.

#### Step 3 — Prepare the SD card

Insert a FAT32-formatted MicroSD card. On first boot the device will attempt to read configuration files from the SD card. If they are not found, it falls back to SPIFFS and automatically copies its factory defaults to the SD card (`syncConfigSPIFFSToSD`). From that point on, the SD card copy is the primary configuration source.

To use **customised** configurations, place edited copies of `MetricConfig.txt`, `SettingConfig.txt`, `CurveConfig.txt`, `AlarmConfig.txt`, and/or `DeviceConfig.txt` on the SD card root before first boot. The device will load those and sync them back to SPIFFS.

### 8.2 TFT_eSPI Library Configuration

The TFT_eSPI library requires a one-time manual setup in its installed library folder. After the first build (which downloads the library), locate the TFT_eSPI folder inside `.pio/libdeps/lilygo-t-display-s3/TFT_eSPI/` and verify:

1. **`User_Setup_Select.h`** — Ensure the LilyGo T-Display S3 setup is enabled:
   ```cpp
   #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
   ```
   All other `#include` lines for other boards should be commented out.

2. Alternatively, the project includes `include/My_Display_Setup.h` which defines:
   - `ST7789_DRIVER`
   - `TFT_WIDTH 170` / `TFT_HEIGHT 320`
   - The correct SPI pin assignments for the T-Display S3

### 8.3 Rebuilding After Configuration Changes

| What changed | Action needed |
|---|---|
| Source code (`.cpp` / `.hpp`) | `pio run --target upload` |
| Files in `data/` folder | `pio run --target uploadfs` (re-flash SPIFFS) |
| Config files on SD card only | No rebuild — just reboot the device |
| `platformio.ini` build flags | Clean build: `pio run --target clean` then `pio run --target upload` |
| TFT_eSPI library settings | Delete `.pio/libdeps/` and rebuild, then re-apply display setup |

---

## 9. File Structure

```
LundaLoggern/
├── platformio.ini              # Build configuration
├── README.md                   # Project overview
├── data/                       # SPIFFS data (uploaded via pio uploadfs)
│   ├── MetricConfig.txt
│   ├── SettingConfig.txt
│   ├── CurveConfig.txt
│   ├── AlarmConfig.txt         # Alarm channel subscriptions (800-999)
│   └── DeviceConfig.txt        # SSID, password, WiFi auto-off
├── Documentation/              # Non-code documentation
├── include/                    # Header files
│   ├── main.hpp                # Global includes, pin defs, constants
│   ├── ServoCIEData.hpp        # CIE protocol & data management
│   ├── SDmanager.hpp           # SD card abstraction
│   ├── WifiApServer.hpp        # WiFi AP & HTTP server
│   ├── ImageRenderer.hpp       # TFT display rendering
│   ├── DateTime.hpp            # Date/time object & RTC
│   ├── Button.hpp              # Button input handler
│   ├── QRHAndler.hpp           # QR code display
│   ├── constants.h             # Display position constants
│   └── ...                     # Fonts, logo bitmap
├── src/                        # Implementation files
│   ├── main.cpp
│   ├── ServoCIEData.cpp
│   ├── SDManager.cpp
│   ├── WifiApServer.cpp
│   ├── ImageRenderer.cpp
│   ├── DateTime.cpp
│   ├── Button.cpp
│   └── QRHandler.cpp
└── LundaLoggern_logo/          # Logo source assets
```

---

## 10. Key Design Decisions

1. **SD + SPIFFS dual storage**: Configurations live in both locations for resilience. SPIFFS holds factory defaults; SD card holds user overrides.
2. **Character-by-character CIE parsing**: A state machine processes each incoming byte, allowing the main loop to remain non-blocking.
3. **Button-controlled WiFi**: WiFi and the web server are off by default to minimise power consumption and RF interference in clinical environments. The operator must explicitly activate them via a long-press of the interaction button, and can shut them down again with a short-press.
4. **Ventilator-derived time**: No RTC module is needed; the device obtains accurate timestamps from the connected ventilator.
5. **Configurable channels**: Which metrics, settings, curves, and alarms to log are fully configurable via text files, enabling adaptation to different clinical needs without recompilation.
6. **Alarm logging**: Alarm data is parsed from the SCI `RADC` continuous data stream (flag byte `0x41` / `'A'`), which transmits priority/status byte pairs whenever any subscribed alarm state changes. Up to 100 alarm channels can be subscribed via the `SDADA` command. Alarms are logged to separate `alarms_YYYYMMDD_HHMM.txt` files with the same 12-hour rolling window as metrics and settings.

---

## 11. Planned Improvements

### 11.1 WiFi Auto-Shutoff — Implementation Analysis

**Goal**: Automatically disable the WiFi access point and web server when no client has been connected for a configurable idle period (default 5 minutes, configurable via `WiFiAutoOff_min` in `DeviceConfig.txt`). This reduces unnecessary RF exposure and power draw in the NICU.

#### Existing behaviour (reference)

The WiFi AP and web server are **off by default**. The operator starts them with a long-press on the interaction button. The shutdown path already exists in [main.cpp](../src/main.cpp) (short-press handler):

```cpp
WiFi.softAPdisconnect(true);
WiFi.mode(WIFI_OFF);
renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");
renderer.drawWiFiPromt("Press key to enable");
```

The ESP32 API `WiFi.softAPgetStationNum()` returns the number of currently connected SoftAP clients — this is already referenced in a comment in `main.cpp`.

#### Implementation plan

The feature requires changes in three areas:

##### A. Load `DeviceConfig.txt` at startup

A new key-value parser (simpler than the channel-config parser) reads `DeviceConfig.txt` from SD (SPIFFS fallback) during `setup()`. Parsed values replace the compile-time `#define LOGGER_SSID` / `#define LOGGER_PW` constants and populate a new `wifiAutoOffMinutes` variable.

**Where**: Add a `loadDeviceConfig()` function. This can live in a small new class (e.g., `DeviceSettings`) or be added to `ServoCIEData` / a free function. The same SD → SPIFFS fallback/sync pattern used by `initializeConfigs()` applies.

**Pseudocode**:
```cpp
struct DeviceSettings {
    String   ssid            = "LundaLoggern";
    String   password        = "neonatal";
    uint16_t wifiAutoOff_min = 5;   // 0 = disabled
};

bool loadDeviceConfig(const char* path, DeviceSettings& cfg);
```

##### B. Track idle time in `WifiApServer`

Add three members to the `WifiApServer` class:

```cpp
// WifiApServer.hpp — new private members
bool     _wifiActive        = false;  // true after begin() is called
uint16_t _autoOffMinutes    = 5;      // 0 = disabled
unsigned long _lastClientSeen = 0;    // millis() timestamp
```

Add a public method to configure and a method to check the idle timer:

```cpp
// WifiApServer.hpp — new public methods
void setAutoOffMinutes(uint16_t minutes);
bool checkAutoOff();  // returns true if WiFi was shut down
```

**`checkAutoOff()` logic** (called from the slow-loop in `main.cpp`):

```
if WiFi is not active, or auto-off is disabled (== 0):
    return false

clientCount = WiFi.softAPgetStationNum()

if clientCount > 0:
    _lastClientSeen = millis()       // reset timer
    return false

if (millis() - _lastClientSeen) > (_autoOffMinutes * 60000):
    shut down WiFi (same code as the short-press handler)
    _wifiActive = false
    return true                      // caller updates display

return false
```

##### C. Integrate into the main loop

In the existing slow-loop block in `main.cpp` (the `if (micros() - loopStartTime > SET_LOOP_TIME)` section), add:

```cpp
if (myWiFiServer.checkAutoOff()) {
    hostCom.println("WiFi auto-off: no clients for timeout period");
    renderer.drawWiFiAPIP("WiFi OFF      ", "No SSID        ");
    renderer.drawWiFiPromt("Press key to enable");
}
```

Also update the long-press handler to pass the loaded SSID/password and reset the idle timer:

```cpp
myWiFiServer.begin();   // already exists
myWiFiServer.resetIdleTimer();  // new — sets _lastClientSeen = millis()
```

#### Summary of changes

| File | Change |
|---|---|
| `data/DeviceConfig.txt` | New file — SSID, Password, WiFiAutoOff_min |
| `main.hpp` | Remove hard-coded `LOGGER_SSID` / `LOGGER_PW` defines; add `DeviceSettings` struct or include |
| `main.cpp` `setup()` | Call `loadDeviceConfig()`; pass loaded SSID/password to `WifiApServer` constructor or a setter |
| `main.cpp` loop | Call `myWiFiServer.checkAutoOff()` in the slow-loop; update display if it returns `true` |
| `WifiApServer.hpp` | Add `_wifiActive`, `_autoOffMinutes`, `_lastClientSeen` members; add `setAutoOffMinutes()`, `checkAutoOff()`, `resetIdleTimer()` methods |
| `WifiApServer.cpp` | Implement `checkAutoOff()` using `WiFi.softAPgetStationNum()` and the shutdown sequence |
| `ServoCIEData.cpp` or new file | `loadDeviceConfig()` with SD → SPIFFS fallback/sync |

#### Edge cases to consider

- **WiFiAutoOff_min = 0**: Feature disabled — WiFi stays on until manual short-press.
- **Client connects then disconnects**: Timer resets on every poll where `clientCount > 0`; countdown only begins after the last client disconnects.
- **WiFi never started**: `checkAutoOff()` returns `false` immediately if `_wifiActive == false`.
- **Operator re-enables WiFi after auto-off**: Long-press works normally, resets the idle timer.
- **Missing DeviceConfig.txt**: Hard-coded defaults (`LundaLoggern` / `neonatal` / 5 min) are used, matching current behaviour.
