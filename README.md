# LundaLoggern

LundaLoggern is a small ESP32-S3 data logger for a SERVO-u intensive care ventilator. It runs on a LilyGo T-Display S3 with a TFT display, SD-card reader, and RS232 transceiver.

The firmware reads channel configuration files from the SD card, mirrors working copies to SPIFFS, communicates with the ventilator over the CIE/SCI serial interface, and writes timestamped tab-separated log files to the SD card. During operation the display shows communication, SD, Wi-Fi, time, and ventilator identity status.

## Build

The project is a PlatformIO Arduino project using the `lilygo-t-display-s3` environment in `platformio.ini`.

Useful commands:

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor -b 115200
```

`uploadfs` uploads the SPIFFS fallback assets and default configuration files.

## Configuration Files

Runtime configuration files live at the root of the SD card and are mirrored to SPIFFS when loaded successfully:

- `/MetricConfig.txt` for breath-by-breath metrics
- `/SettingConfig.txt` for ventilator settings
- `/CurveConfig.txt` for waveform channels
- `/AlarmConfig.txt` for alarm channels

Each non-comment row is tab-separated:

```text
channel	label	unit	scaleFactor	offset	active
```

The `active` field is optional. If omitted, the row is active. If present, only `true` or `1` enables the row. Inactive rows are ignored when building SCI subscription commands and are not part of the runtime channel arrays.

If the SD card is missing or a config file is unavailable, the firmware falls back to SPIFFS copies. If a config file is found on SD, that version is mirrored to SPIFFS for later fallback use.

## Logging

Log files are written to the SD-card root in 12-hour intervals using these prefixes:

- `metrics_YYYYMMDD_HHMM.txt`
- `settings_YYYYMMDD_HHMM.txt`
- `alarms_YYYYMMDD_HHMM.txt`

Log timestamps come from the ventilator clock after a valid `RTIM` response. If the response is invalid, the RTC is left unchanged.

Curve samples are currently available through the USB Host CLI stream when `STREAM ON C` is enabled.

## Web Interface

A long press on the interaction key starts the Wi-Fi access point.

- SSID: `LundaLoggern`
- AP password: configured by `LOGGER_PW` in `include/main.hpp`
- Web file/config username: configured by `LOGGER_WEB_USER`
- Web file/config password: configured by `LOGGER_WEB_PW`

The main status page is open to clients connected to the AP. File manager, download/delete actions, and configuration viewing require HTTP Basic authentication. The file routes only accept root-level SD filenames and reject path traversal characters.

## USB Host CLI

The USB serial interface starts at 115200 baud and prints a `READY` line on boot. Type `HELP` for commands. Streams are off at boot and can be enabled with commands such as:

```text
STREAM ON M,S,A
STREAM ON C
STATUS
LIST
DUMP metrics_YYYYMMDD_HHMM.txt
```

## Notes

- TFT_eSPI must be configured for the LilyGo T-Display S3 setup noted in `src/main.cpp`.
- `ARDUINO_USB_CDC_ON_BOOT` and `ARDUINO_USB_MODE` are enabled in `platformio.ini` for USB-CDC host communication.
- PSRAM is enabled in `platformio.ini`.
