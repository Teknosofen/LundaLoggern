#ifndef SERVOCIE_DATA_HPP
#define SERVOCIE_DATA_HPP

// =============================================================================
//  ServoCIEData.hpp — SERVO ventilator CIE/SCI protocol driver and channel data
// -----------------------------------------------------------------------------
//  Talks to a SERVO-u/n/air ventilator over its Servo Communication Interface
//  (SCI, also called CIE). Reference: Documentation/
//  87498-servo-connectivity-reference-manual-sci-v0010.pdf (section 2.4).
//
//  Physical link: RS232, 38400 baud, 8E1 (opened in setup()).
//
//  Commands (logger -> ventilator) are ASCII:
//      <COMMAND><payload><CHK><EOT>
//  where CHK is the XOR of all command+payload characters, sent as two ASCII
//  hex digits, and EOT = 0x04. Replies to ASCII commands use the same framing.
//
//  Init sequence (CIE_setup()):
//      <EOT>           empty command = link check, reply "*2A<EOT>"
//      RTIM            ventilator clock -> seeds the ESP32 system clock
//      RCTY, RSEN      device type and serial number (device ID for the logs)
//      RHVE, SPVE      read highest protocol version and switch to it
//      SDADB/S/A/C     subscribe to breath / settings / alarm / curve channels
//      RCCO, RDAD,     diagnostic reads (replies printed or discarded)
//      RADAB, RADAS
//      RADC            start continuous binary streaming
//  Order matters: RCTY and SPVE both clear the subscription table, so the
//  SDAD* commands must come after them.
//
//  Continuous stream (RADC) is binary and parsed byte by byte by
//  parseCIEData(). Every frame starts with a type byte and ends with the end
//  flag 0x7F followed by one binary checksum byte:
//      'B' <v0><v1>...  0x7F <CHK>   breath data (metrics), one per breath
//      'S' <v0><v1>...  0x7F <CHK>   settings, sent when a setting changes
//      'A' <p0><s0>...  0x7F <CHK>   alarms: priority/status byte pairs
//      0x81 <phase>                  breath phase (0x10 insp, 0x20 pause, 0x30 exp)
//      0x80 <hi><lo>                 one absolute curve sample
//      0xE0 <code>      0x7F <CHK>   error / stream stopped
//  Data values are 16-bit, MSB first, in the range 0x0000..0x7EFF.
//  0x7EFF means "no valid data". Because a high byte is therefore never 0x7F,
//  the parser can tell an end flag from the start of the next value.
//  Values are listed in the same order as the channels in the matching
//  SDAD* command, which is the order of the entries in the config file.
//
//  Channel configuration
//  ---------------------
//  Which channels to log, and how to scale them, comes from tab-separated
//  config files (MetricConfig.txt, SettingConfig.txt, CurveConfig.txt,
//  AlarmConfig.txt), read from SD with SPIFFS as fallback. Each parsed line
//  becomes one Configs entry; see initializeConfigs().
//
//  Output
//  ------
//  Complete breath/settings/alarm frames are written as one tab-separated row
//  to SD via SDManager, and offered to HostCli for USB streaming. Curve samples
//  go only to HostCli (USB); they are not written to SD.
// =============================================================================

#include <Arduino.h>
#include "main.hpp"
#include <FS.h>
#include <SPIFFS.h>
#include <string>
#include "DateTime.hpp"
#include "SDManager.hpp"  // Include the header for SDManager

extern DateTime dateTime;
extern WifiApServer WiFiserver; // NOTE: no object with this name exists (the global is myWiFiServer); unused

class SDManager; // forward declaration

/// One configured data channel: one line of a *Config.txt file plus the
/// latest value received for it.
///
/// Config file line format (tab-separated, '#' starts a comment line):
///     <channel> <label> <unit> <scaleFactor> <offset> [<active>]
/// e.g. "113	VtCO2	ml	0.1	0.0"
///
/// Scaling as implemented by the frame handlers:
///     scaled = unscaled * scaleFactor - offset
///
/// Alarm channels reuse the value fields differently:
///     unscaled = alarm status (0 none, 1 active, 2 silenced),
///                or cieDataInvalid when the alarm does not apply
///     scaled   = alarm priority (0 undefined, 1 low, 2 medium, 3 high),
///                or -1 when the alarm does not apply
struct Configs {
    String channel = "000";     // SCI channel number, always 3 digits ("113")
    String label = "default";   // column name used in log headers
    String unit = "N/A";        // engineering unit (informational only)
    float scaleFactor = 1.0;
    float offset = 0.0;
    uint16_t unscaled = 1;      // raw incoming value
    float scaled = 1;           // scaled/calibrated value
    bool active = true;         // inactive lines are skipped when loading
};

class ServoCIEData {

public:
    /// @param manager SD card manager used to write the log rows. Must outlive this object.
    ServoCIEData(SDManager* manager);

    /// Stop any ongoing transmission (ESC), flush the UART, and run CIE_setup().
    /// Always returns true; check isComOpen() for the actual link state.
    bool begin();

    /// Feed one received byte to the stream state machine. Call for every byte
    /// read from servoCom. Complete frames are logged as a side effect.
    void parseCIEData(char NextSCI_chr);

    /// Collect the reply to an ASCII command: reads until EOT, 500 ms total,
    /// or 50 ms of silence after the first byte. Blocking. Echoes the bytes to hostCom.
    String getCIEResponse();

    // --- Parsers for ASCII command replies ---
    /// Parse "YYYYMMDDhhmmss" from an RTIM reply and set the ESP32 system clock.
    /// Returns an invalid DateTime on CRC or length error.
    DateTime parseRTIMResponse(const char* response, size_t len);
    /// Parse the device type (e.g. "SERVO-U") from an RCTY reply into servoID.
    void parseRCTYResponse(const char* response, size_t len);
    /// Parse the serial number from an RSEN reply into servoSN.
    void parseRSENResponse(const char* response, size_t len);

    /// Link check with the SCI "empty command": send ESC, flush, send EOT,
    /// and expect "*2A<EOT>" within 500 ms. Returns true if the ventilator answered.
    bool CIE_comCheck();

    /// Full init sequence (see file header): build all command strings, check
    /// the link, read time/type/serial, select the protocol version, subscribe
    /// to the configured channels, and start RADC streaming.
    /// Returns false if a command does not fit its buffer or the link check fails.
    bool CIE_setup();

    // --- Command sending ---
    /// Legacy sender with a binary CRC byte. Unused; the SCI expects ASCII
    /// CRC, see Send_SERVO_CMD_ASCII().
    void Send_SERVO_CMD(const char* InStr);
    /// Send <InStr><CHK as 2 ASCII hex digits><EOT> to the ventilator.
    void Send_SERVO_CMD_ASCII(const char* InStr);

    // --- CRC ---
    /// XOR of all characters of a null-terminated string (the SCI checksum).
    uint8_t CRC_calc(const char* localstring);
    /// SCI checksum as two upper-case ASCII hex characters; outHex needs room for 3 chars.
    void CRC_calcASCII(const char* localstring, char* outHex);

    /// Concatenate the channel numbers of all active entries, e.g. "113114117",
    /// as payload for the SDAD* subscription commands.
    String concatConfigChannels(const Configs configs[], int numConfigs);

    /// Load all channel config files. For each file: try SD; on success copy it
    /// to SPIFFS, otherwise load the SPIFFS copy and write it back to SD as a
    /// template. A file only counts as loaded if it has at least one valid line.
    /// Without an SD card, only SPIFFS is used. Results are printed to hostCom.
    void initializeConfigs(const char* metricPath, const char* settingPath, const char* curvePath = nullptr, const char* alarmPath = nullptr);

    // --- Link status (driven by main.cpp's supervision in loop()) ---
    void setComOpen(bool state);
    bool isComOpen() const;

    /// millis() of the last init attempt; used to space out re-init retries.
    void setLastInitAttempt(unsigned long timestamp);
    unsigned long getLastInitAttempt() const;

    /// millis() of the last complete frame; used for the link timeout.
    void setLastMessageTime(unsigned long timestamp);
    unsigned long getLastMessageTime() const;

    // --- Channel tables --------------------------------------------------------
    // Public so HostCli can read labels and values. Entries [0 .. xxxCount-1]
    // are valid, in config-file order, which is also the order of the values in
    // the ventilator's frames.
    static const int MaxMetrics = 20;
    static const int MaxSettings = 20;
    static const int MaxCurves = 5;   // note: handleValueData() only handles 4 curve slots

    Configs metrics[MaxMetrics];
    int metricCount = 0;
    bool metricConfigLoaded = false;  // NOTE: never set (initializeConfigs() uses a local of the same name)

    Configs settings[MaxSettings];
    int settingCount = 0;
    bool settingConfigLoaded = false; // NOTE: never set, see above

    Configs curves[MaxCurves];
    int curveCount = 0;
    bool curveConfigLoaded = false;   // NOTE: never set, see above

    static const int MaxAlarms = 100; // SCI limit for SDADA
    Configs alarms[MaxAlarms];
    int alarmCount = 0;
    bool alarmConfigLoaded = false;

    // --- Config file handling ---
    /// Parse a config file from SD into `configs`. Skips blank, '#' and inactive
    /// lines; stops storing at maxConfigs. Returns false if the file is missing
    /// or has no valid entries.
    bool loadConfigFromSD(const char* path, Configs* configs, int& configCount, const int maxConfigs);
    /// Same as loadConfigFromSD(), reading from SPIFFS.
    bool loadConfigFromSPIFFS(const char* path, Configs* configs, int& configCount, const int maxConfigs);
    /// Byte-copy a file from SD to SPIFFS (same path), overwriting.
    bool syncConfigSDToSPIFFS(const char* path);
    /// Byte-copy a file from SPIFFS to SD (same path), overwriting.
    bool syncConfigSPIFFSToSD(const char* path);

    /// Print a config table to hostCom (debug).
    void printAllConfigs(const Configs* configs, const int numConfigs, const char* ConfigPaths);
    // Declared but not implemented:
    void printAllCurveConfigs();
    void printAllMetricsConfigs();
    void printAllSettingConfigs();

    // --- Formatting helpers for log rows. All join with '\t' and end with '\n'. ---
    /// Unused helper. Note: it ADDS the offset, unlike the frame handlers.
    void scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray);
    String getUnitsAsString(const Configs* configsArray, int count) ;
    String getLabelsAsString(const Configs* configsArray, int count);
    /// Scaled values with 2 decimals; raw 0x7EFF is written as "No Valid Data".
    /// With includeHeader, a label line is prepended.
    String getScaledValuesAsString(const Configs* configsArray, int count, bool includeHeader = false);
    String getChannelsAsString(const Configs* configsArray, int count);

    /// Device type + serial number, e.g. "SERVO-U12345". "SERVO---" until the ventilator has been read.
    String getServoID();
    /// Latest breath phase byte: 0x10 insp, 0x20 pause, 0x30 exp (0 = none received yet).
    uint8_t getBreathPhase();

    // --- Alarm helpers ---
    String getAlarmLabelsAsString();
    /// One "prio:status" field per alarm channel, or "N/A" if not applicable.
    String getAlarmValuesAsString();
    /// True if any configured alarm is active or silenced.
    bool hasActiveAlarm() const;

private:

    SDManager* sdManager;  // where log rows go; not owned

    // --- Stream state machine: one handler per RunMode, see parseCIEData() ---
    void handleAwaitingInfo(uint8_t NextSCI_chr);
    void handleBreathData(uint8_t NextSCI_chr);
    void handleSettingsData(uint8_t NextSCI_chr);
    void handleValueData(uint8_t NextSCI_chr);
    void handlePhaseData(uint8_t NextSCI_chr);
    void handleTrendData(uint8_t NextSCI_chr);
    void handleAlarmData(uint8_t NextSCI_chr);
    void handleErrorData(uint8_t NextSCI_chr);
    void handleRunMode(uint8_t NextSCI_chr);

    // --- Link timing (millis() values) ---
    bool comOpen = false;
    unsigned long lastMessageTime = 0;
    unsigned long lastInitAttempt = 0;

    /// Split one tab-separated config line into `c`. Needs at least 5 fields.
    bool parseConfigLine(const String& line, Configs& c);

    /// Check EOT and the 2-digit ASCII CRC of a command reply and return the
    /// payload without CRC/EOT. Returns "" and crcError=true if invalid.
    String parseASCIIResponse(const char* response, size_t len, bool& crcError);
    /// Like parseASCIIResponse(), for replies whose last payload character is a
    /// status digit ('1' = internal communication error). The status digit is removed.
    String parseASCIIStatusResponse(const char* response, size_t len, bool& crcError, bool& statusError);
    /// Write prefix+payload into outBuffer. Returns false (and an empty buffer)
    /// if it does not fit, e.g. too many channels for the command buffer.
    bool buildCommand(char* outBuffer, size_t outSize, const char* prefix, const String& payload);
    /// Abandon the current frame (e.g. more values than configured channels)
    /// and wait for the next frame type byte.
    void resetDataFrame(const char* reason);

    // Parsed device info
    String servoID = "SERVO";  // device type, from RCTY
    String servoSN = "---";    // serial number, from RSEN

    /// What the parser expects next. See the handlers for the byte layout.
    enum RunModeType {
        Awaiting_Info,   // between frames: waiting for a frame type byte
        End_Flag_Found,  // end flag seen: the next byte is the frame checksum (skipped)
        Breath_Data,     // inside a 'B' frame
        Value_Data,      // 2 bytes of a curve sample follow (after 0x80)
        Phase_Data,      // 1 breath phase byte follows (after 0x81)
        Trend_Data,      // inside a 'T' frame: skipped until the end flag
        Alarm_Data,      // inside an 'A' frame
        Error_Data,      // inside an error frame (after 0xE0)
        Settings_Data,   // inside an 'S' frame
        Run_Mode,        // inside curve streaming: waiting for 0x80 / 0x81 / 0x7F
        StandBy          // unused
    };

    static constexpr char EOT = 0x04;
    static constexpr char ESC = 0x1B;

    // --- Command buffers ---------------------------------------------------------
    // Filled once per CIE_setup() by buildCommand(). Each holds the full ASCII
    // command without CRC/EOT (Send_SERVO_CMD_ASCII() appends those). Sizes
    // bound the number of subscribed channels: 3 characters per channel.
    char CMD_RTIM[8];
    char CMD_RCTY[8];
    char CMD_RSEN[8];
    char CMD_RHVE[8];
    char CMD_SPVE[16];
    char CMD_SDADB[72];       // "SDADB" + up to 20 metric channels
    char PAYLOAD_SDADB[64];   // unused
    char PAYLOAD_SDADS[64];   // unused
    char CMD_SDADS[72];       // "SDADS" + up to 20 setting channels
    char CMD_SDADC[32];
    char PAYLOAD_SDADC[32];
    char CMD_RCCO[16];
    char CMD_RDAD[8];
    char CMD_RADAB[8];
    char CMD_RADAS[8];
    char CMD_RADC[8];
    char CMD_SDADA[400];  // SDADA + up to 100 alarm channel numbers

    char tempCommand[128];    // unused


    RunModeType RunMode;
    int ByteCount;     // bytes still expected for the current value (2 = high byte next, 1 = low byte next)
    int phase;         // unused
    int Error_info;    // last error code from an error frame
    char inByte;       // last byte read by getCIEResponse()

    // Accumulators for the curve sample being received, one per curve slot.
    // TODO: could be replaced by curves[n].unscaled directly.
    uint16_t cieCurve1 = 0; // curve slot 0
    uint16_t cieCurve2 = 0; // curve slot 1
    uint16_t cieCurve3 = 0; // curve slot 2
    uint16_t cieCurve4 = 0; // curve slot 3
    uint16_t cieCurve5 = 0; // unused (no handler for slot 4)

    uint8_t breathPhase = 0;   // latest phase byte, see getBreathPhase()

    unsigned int MetricNo = 0;      // index of the metric value being received
    unsigned int settingsNo = 0;    // index of the setting value being received
    unsigned int CurveCounter = 0;  // index of the curve slot the next sample belongs to

    // Alarm frames carry <prio><status> byte pairs, one pair per alarm channel.
    unsigned int alarmNo = 0;         // index of the alarm pair being received
    bool alarmPrioReceived = false;   // true after the first byte of a pair
    uint8_t currentAlarmPrio = 0;     // first byte of the current pair (priority)


    // --- SCI protocol constants ------------------------------------------------
    // Note: these #defines are global (preprocessor macros ignore class scope),
    // and EOT/ESC here also replace the constexpr members above in the code
    // that follows.
    #define EOT 0x04  // end of transmission: terminates ASCII commands/replies
    #define ESC 0x1B  // escape: makes the ventilator stop any ongoing output
    #define CR  0x0D   // CR char
    #define LF  0x0A   // LF char
    #define ValueFlag 0x80  // curve stream: absolute 16-bit sample follows
    #define PhaseFlag 0x81  // curve stream: breath phase byte follows
    #define ErrorFlag 0xE0  // error frame: error code byte follows
    #define EndFlag   0x7F  // end of frame (followed by one checksum byte)

    // Error codes carried in error frames (0xE0 <code>)
    #define StdbyErr    0x11 // ventilator is in standby: streaming stopped
    #define BuffFullErr 0x13 // ventilator output buffer overflowed: streaming stopped
    #define ESCErr      0x14 // streaming stopped because we sent ESC


    #define cieDataInvalid 0x7EFF  // 32511: "no valid data" / "alarm not applicable"
    #define cieEIPInvalid 3000.0 // unused; limit just below a scaled 0x7EFF
};

#endif // SERVOCIE_DATA_HPP
