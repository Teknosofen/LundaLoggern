// =============================================================================
//  ServoCIEData.cpp — SERVO CIE/SCI protocol driver
// -----------------------------------------------------------------------------
//  Protocol overview, frame layouts and the init sequence are described in
//  ServoCIEData.hpp. This file is organised as:
//    1. construction and begin()
//    2. stream state machine (parseCIEData() and one handler per RunMode)
//    3. command sending, CRC and reply parsing
//    4. link check and the init sequence (CIE_comCheck(), CIE_setup())
//    5. channel configuration files
//    6. formatting helpers for log rows
// =============================================================================

#include "ServoCIEData.hpp"
#include "DateTime.hpp"
#include "SDManager.hpp"

// =============================================================================
// 1. Construction and begin()
// =============================================================================

ServoCIEData::ServoCIEData(SDManager* manager)
    : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0), sdManager(manager) {
}

bool ServoCIEData::begin() {
    hostCom.println("ServoCIEData begin called");

    // ESC stops any streaming left over from a previous session (e.g. after
    // an ESP32 reset while the ventilator kept sending), then drop whatever
    // is already in the receive buffer so the init replies start clean.
    servoCom.write(ESC);
    hostCom.printf("Flushed after ESC: ");
    while (servoCom.available() > 0) {
        uint8_t b = servoCom.read();
    }
    hostCom.println("");
    CIE_setup();
    return true;
}

// =============================================================================
// 2. Stream state machine
// -----------------------------------------------------------------------------
// parseCIEData() is called once per received byte and dispatches on RunMode.
// Each handler consumes the byte and decides what comes next. Typical flow:
//
//   Awaiting_Info --'B'--> Breath_Data   --0x7F--> End_Flag_Found --CHK--> Awaiting_Info
//   Awaiting_Info --'S'--> Settings_Data --0x7F--> End_Flag_Found --CHK--> Awaiting_Info
//   Awaiting_Info --'A'--> Alarm_Data    --0x7F--> End_Flag_Found --CHK--> Awaiting_Info
//   Awaiting_Info --0x80-> Value_Data --2 bytes--> Run_Mode --0x80--> Value_Data ...
//                                                  Run_Mode --0x81--> Phase_Data --> Run_Mode
//                                                  Run_Mode --0x7F--> End_Flag_Found ...
//
// The ventilator interrupts the curve stream to send breath/settings/alarm
// frames; it ends the curve part with 0x7F <CHK> first, so the parser is back
// in Awaiting_Info when the next frame type byte arrives.
// =============================================================================

void ServoCIEData::parseCIEData(char NextSCI_chr) {
    switch (RunMode) {
        case Awaiting_Info:   handleAwaitingInfo(NextSCI_chr);   break;
        case End_Flag_Found:  RunMode = Awaiting_Info;           break;  // this byte is the frame checksum: skip it
        case Breath_Data:     handleBreathData(NextSCI_chr);     break;
        case Settings_Data:   handleSettingsData(NextSCI_chr);   break;
        case Value_Data:      handleValueData(NextSCI_chr);      break;
        case Phase_Data:      handlePhaseData(NextSCI_chr);      break;
        case Trend_Data:      handleTrendData(NextSCI_chr);      break;
        case Alarm_Data:      handleAlarmData(NextSCI_chr);      break;
        case Error_Data:      handleErrorData(NextSCI_chr);      break;
        case Run_Mode:        handleRunMode(NextSCI_chr);        break;
        default:              break;
    }
}

// Between frames: the byte identifies the next frame type. Unknown bytes are
// ignored, which is also how the parser resynchronises after garbage.
// ByteCount = 2 means "the next byte is the high byte of the first value".
void ServoCIEData::handleAwaitingInfo(uint8_t NextSCI_chr) {
    switch (NextSCI_chr) {
        case 'S':  RunMode = Settings_Data; settingsNo = 0; ByteCount = 2; break;
        case 'B':  RunMode = Breath_Data;   MetricNo = 0;  ByteCount = 2; break;
        case 'T':  RunMode = Trend_Data;                                break;
        case 'A':  RunMode = Alarm_Data;                                hostCli.dbg("Alarm 'A' frame start"); break;
        case 0xE0: RunMode = Error_Data;    ByteCount = 1;               break;
        case 0x80: RunMode = Value_Data;    ByteCount = 2;               break;
        case 0x81: RunMode = Phase_Data;    ByteCount = 1;               break;
        case 0x7F: RunMode = Awaiting_Info;                             break;
        default:   break;
    }
}

void ServoCIEData::resetDataFrame(const char* reason) {
    hostCli.dbg(String("CIE frame reset: ") + reason);
    RunMode = Awaiting_Info;
    ByteCount = 0;
    MetricNo = 0;
    settingsNo = 0;
}

// 'B' frame: <hi0><lo0><hi1><lo1>... 0x7F <CHK>, one value per subscribed
// metric channel, in MetricConfig.txt order.
//   ByteCount == 2 : high byte of the first value (only right after 'B')
//   ByteCount == 1 : low byte; the value is now complete and gets scaled
//   ByteCount == 0 : either the end flag, or the high byte of the next value.
//                    No ambiguity: a high byte is at most 0x7E.
// A frame with more values than configured channels is dropped (bounds check)
// rather than overrunning metrics[].
void ServoCIEData::handleBreathData(uint8_t NextSCI_chr) {
    if (ByteCount == 2) {
        if (MetricNo >= (unsigned int)metricCount || MetricNo >= (unsigned int)MaxMetrics) {
            resetDataFrame("metric overflow");
            return;
        }
        metrics[MetricNo].unscaled = 256 * NextSCI_chr;
        --ByteCount;
    } else if (ByteCount == 1) {
        if (MetricNo >= (unsigned int)metricCount || MetricNo >= (unsigned int)MaxMetrics) {
            resetDataFrame("metric overflow");
            return;
        }
        metrics[MetricNo].unscaled += NextSCI_chr;
        --ByteCount;
        metrics[MetricNo].scaled = metrics[MetricNo].unscaled * metrics[MetricNo].scaleFactor - metrics[MetricNo].offset;
    } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        // Frame complete: write one row "<timestamp>\t<v0>\t<v1>...".
        // If this 12-hour log file is new or empty, write the file header
        // (device ID line + column labels) first.
        const int receivedCount = MetricNo + 1;
        String newFilename, dataStr;
        bool isNewFile = sdManager->updateFileNameIfChanged('M', newFilename);
        if (isNewFile) {
            dataStr = "Data from: " + getServoID() + "\n";
            dataStr += "Timestamp\t" + getLabelsAsString(metrics, receivedCount);   // ends with '\n'
            sdManager->appendData(dataStr, 'M');
            hostCli.emitMetric(dataStr);
        }
        dataStr = dateTime.getRTC().toString() + "\t" + getScaledValuesAsString(metrics, receivedCount);
        sdManager->appendData(dataStr, 'M');
        setLastMessageTime(millis());   // link is alive
        hostCli.emitMetric(dataStr);    // USB mirror, only if STREAM M is on
        RunMode = End_Flag_Found;       // next byte is the frame checksum
    } else {
        // High byte of the next value.
        MetricNo++;
        if (MetricNo >= (unsigned int)metricCount || MetricNo >= (unsigned int)MaxMetrics) {
            resetDataFrame("metric overflow");
            return;
        }
        metrics[MetricNo].unscaled = 256 * NextSCI_chr;
        ByteCount = 1;
    }
}

// 'S' frame: same layout and byte handling as the 'B' frame above, for the
// channels in SettingConfig.txt. The ventilator sends it when a setting changes.
void ServoCIEData::handleSettingsData(uint8_t NextSCI_chr) {
    if (ByteCount == 2) {
        if (settingsNo >= (unsigned int)settingCount || settingsNo >= (unsigned int)MaxSettings) {
            resetDataFrame("setting overflow");
            return;
        }
        settings[settingsNo].unscaled = 256 * NextSCI_chr;
        --ByteCount;
    } else if (ByteCount == 1) {
        if (settingsNo >= (unsigned int)settingCount || settingsNo >= (unsigned int)MaxSettings) {
            resetDataFrame("setting overflow");
            return;
        }
        settings[settingsNo].unscaled += NextSCI_chr;
        --ByteCount;
        settings[settingsNo].scaled = settings[settingsNo].unscaled * settings[settingsNo].scaleFactor - settings[settingsNo].offset;
    } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        const int receivedCount = settingsNo + 1;
        String newFilename, dataStr;
        bool isNewFile = sdManager->updateFileNameIfChanged('S', newFilename);
        if (isNewFile) {
            dataStr = "Data from: " + getServoID() + "\n";
            dataStr += "Timestamp\t" + getLabelsAsString(settings, receivedCount);  // ends with '\n'
            sdManager->appendData(dataStr, 'S');
            hostCli.emitSetting(dataStr);
        }
        // One row per settings frame, same layout as the metrics log:
        // "<timestamp>\t<v0>\t<v1>..."; the labels are in the file header.
        dataStr = dateTime.getRTC().toString() + "\t" + getScaledValuesAsString(settings, receivedCount);
        sdManager->appendData(dataStr, 'S');
        setLastMessageTime(millis());
        hostCli.emitSetting(dataStr);
        RunMode = End_Flag_Found;
    } else {
        settingsNo++;
        if (settingsNo >= (unsigned int)settingCount || settingsNo >= (unsigned int)MaxSettings) {
            resetDataFrame("setting overflow");
            return;
        }
        settings[settingsNo].unscaled = 256 * NextSCI_chr;
        ByteCount = 1;
    }
}

// Absolute curve sample after a 0x80 value flag: <hi><lo>.
void ServoCIEData::handleValueData(uint8_t NextSCI_chr) {
    if (ByteCount == 2) {
        curveAccum = (uint16_t)NextSCI_chr << 8;
        ByteCount = 1;
    } else {
        curveAccum |= NextSCI_chr;
        ByteCount = 0;
        RunMode = Run_Mode;
        storeCurveSample(curveAccum);
    }
}

// Store one curve sample in slot CurveCounter and advance to the next slot.
// Samples arrive round-robin over the subscribed curve channels, and SDADC
// subscribes exactly the channels of CurveConfig.txt in file order (see
// CIE_setup()), so slot n is curves[n]. When the last slot has been filled,
// CurveCounter wraps to 0 and the complete sample set is offered to HostCli
// (USB only; curves are not logged to SD).
// `valid` = false skips the slot without changing its value (a delta sample
// arrived for a slot that has no absolute base value yet).
void ServoCIEData::storeCurveSample(uint16_t raw, bool valid) {
    if (curveCount <= 0) return;                 // no curves subscribed
    if (CurveCounter >= (unsigned int)curveCount) CurveCounter = 0;

    Configs& c = curves[CurveCounter];
    if (valid) {
        c.unscaled = raw;
        c.scaled   = raw * c.scaleFactor - c.offset;
        curveHasBase[CurveCounter] = true;
    }

    if (++CurveCounter >= (unsigned int)curveCount) {
        CurveCounter = 0;
        setLastMessageTime(millis());            // link is alive
        hostCli.emitCurveSample();
    }
}

// Breath phase after a 0x81 phase flag: 0x10 insp, 0x20 pause, 0x30 exp.
// Sent once at stream start and then only when the phase changes.
void ServoCIEData::handlePhaseData(uint8_t NextSCI_chr) {
    breathPhase = NextSCI_chr;
    --ByteCount;
    RunMode = Run_Mode;
}

// 'T' frame: content is not used; skip to the end flag.
void ServoCIEData::handleTrendData(uint8_t NextSCI_chr) {
    if (NextSCI_chr == EndFlag) {
        RunMode = End_Flag_Found;
    }
}

// 'A' frame: <prio0><status0><prio1><status1>... 0x7F <CHK>, one byte pair per
// subscribed alarm channel (AlarmConfig.txt order). The ventilator only sends
// it when a subscribed alarm changes, so each frame is logged as a snapshot.
//   prio   : 0 undefined, 1 low, 2 medium, 3 high
//   status : 0 no alarm, 1 active, 2 active but silenced
//   pair == 0x7E 0xFF : alarm not applicable in the current mode
// Neither byte of a pair can be 0x7F, so the end flag is unambiguous here too.
void ServoCIEData::handleAlarmData(uint8_t NextSCI_chr) {
    if (NextSCI_chr == EndFlag) {
        // All pairs received: write one row "<timestamp>\t<prio:status>...".
        String newFilename, dataStr;
        bool isNewFile = sdManager->updateFileNameIfChanged('A', newFilename);
        if (isNewFile) {
            dataStr = "Data from: " + getServoID() + "\n";
            dataStr += "Timestamp\t" + getAlarmLabelsAsString();
            sdManager->appendData(dataStr, 'A');
            hostCli.emitAlarm(dataStr);
        }
        dataStr = dateTime.getRTC().toString() + "\t" + getAlarmValuesAsString();
        sdManager->appendData(dataStr, 'A');
        setLastMessageTime(millis());
        hostCli.emitAlarm(dataStr);

        // Reset pair tracking here; the 'A' frame start does not reset it.
        alarmNo = 0;
        alarmPrioReceived = false;
        RunMode = End_Flag_Found;
        return;
    }

    if (!alarmPrioReceived) {
        // First byte of the pair: priority. Kept until the second byte arrives.
        currentAlarmPrio = NextSCI_chr;
        alarmPrioReceived = true;
    } else {
        // Second byte of the pair: status. Pairs beyond alarmCount are ignored.
        if (alarmNo < (unsigned int)alarmCount) {
            uint16_t combined = (currentAlarmPrio << 8) | NextSCI_chr;
            if (combined == cieDataInvalid) {
                alarms[alarmNo].unscaled = cieDataInvalid;
                alarms[alarmNo].scaled = -1;  // flag as N/A
            } else {
                alarms[alarmNo].unscaled = NextSCI_chr;      // status: 0=No alarm, 1=Active, 2=Silenced
                alarms[alarmNo].scaled = currentAlarmPrio;  // priority, stored in the "scaled" field
            }
        }
        alarmNo++;
        alarmPrioReceived = false;
    }
}

// Error frame after 0xE0: <code> 0x7F <CHK>. Standby stops the stream, so the
// link is marked closed; main.cpp then retries the init periodically.
void ServoCIEData::handleErrorData(uint8_t NextSCI_chr) {
    if (NextSCI_chr == EndFlag) {
        RunMode = End_Flag_Found;
    } else {
        Error_info = NextSCI_chr;
        if (Error_info == StdbyErr) {
            hostCom.println(" CIE in stdby");
            setComOpen(false);
        } else if (Error_info == BuffFullErr) {
            hostCom.println(" CIE buffer full");
        } else if (Error_info == ESCErr) {
            hostCom.println(" CIE transmission stopped by ESC");
        } else {
            hostCom.print(" CIE error code: ");
            hostCom.println(Error_info);
        }
    }
}

// Inside the curve stream, between samples. Apart from the three flags, every
// byte is a one-byte delta sample <diff_value>: a signed 8-bit difference
// (0x82..0x7E = -126..+126) from the previous sample of the same channel.
// The ventilator sends an absolute sample (0x80 <hi><lo>) first and whenever
// the difference is too large, including after every interruption.
void ServoCIEData::handleRunMode(uint8_t NextSCI_chr) {
    switch (NextSCI_chr) {
        case ValueFlag: RunMode = Value_Data; ByteCount = 2; break;
        case PhaseFlag: RunMode = Phase_Data; ByteCount = 1; break;
        case EndFlag:   RunMode = End_Flag_Found;            break;
        default: {
            if (curveCount <= 0) break;
            if (CurveCounter >= (unsigned int)curveCount) CurveCounter = 0;
            int value = (int)curves[CurveCounter].unscaled + (int8_t)NextSCI_chr;
            bool valid = curveHasBase[CurveCounter] && value >= 0 && value <= cieDataInvalid;
            storeCurveSample((uint16_t)value, valid);
            break;
        }
    }
}


// =============================================================================
// 3. Command sending, CRC and reply parsing
// =============================================================================

// SCI checksum: XOR of all characters.
uint8_t ServoCIEData::CRC_calc(const char* localstring) {
    char chk = 0;
    for (size_t i = 0; localstring[i] != '\0'; i++) {
        chk ^= localstring[i];
    }
    return chk;
}

// SCI checksum as it is sent on the wire: two upper-case ASCII hex characters.
void ServoCIEData::CRC_calcASCII(const char* localstring, char* outHex) {
    uint8_t chk = CRC_calc(localstring);
    snprintf(outHex, 3, "%02X", chk);     // outHex[0..1] = hex chars, outHex[2] = '\0'
}

// Legacy: sends the CRC as a raw byte (and "0" instead of the CRC when it is
// below 0x10). Not used; the SCI expects the ASCII form, see Send_SERVO_CMD_ASCII().
void ServoCIEData::Send_SERVO_CMD(const char* InStr) {
    char CRC = CRC_calc(InStr);
    servoCom.print(InStr);
    if (CRC < 0x10) {
        servoCom.print("0");
    } else {
        servoCom.write(CRC);
    }
    servoCom.write(EOT);

    hostCom.printf("CRC for %s = 0x%02X\n", InStr, CRC);
}

// Send "<InStr><CHK hi><CHK lo><EOT>", e.g. "RTIM" -> "RTIM02" EOT.
void ServoCIEData::Send_SERVO_CMD_ASCII(const char* InStr) {
    char crcStr[3];                  // 2 ASCII chars + null terminator
    CRC_calcASCII(InStr, crcStr);

    servoCom.print(InStr);
    servoCom.print(crcStr);
    servoCom.write(EOT);

    hostCom.printf("Sent command: %s, CRC ASCII: %s\n", InStr, crcStr);
}

// Blocking read of one command reply. Stops at EOT, after 500 ms in total, or
// after 50 ms without a new byte (once something has arrived). Each byte is
// echoed to hostCom as char and hex, which makes the init sequence easy to
// follow in a terminal.
String ServoCIEData::getCIEResponse() {
    String response = "";
    const unsigned long timeoutMs = 500;
    const unsigned long idleTimeoutMs = 50;
    unsigned long startTime = millis();
    unsigned long lastByteTime = startTime;
    hostCom.print("CIE data: ");
    while ((millis() - startTime) < timeoutMs) {
        while (servoCom.available()) {
            inByte = servoCom.read();
            lastByteTime = millis();
            hostCom.print(inByte);
            hostCom.print(" (0x");
            hostCom.print(inByte, HEX);
            hostCom.print(") ");
            response += (char)inByte;
            if (inByte == EOT) {
                hostCom.println(" All CIE data received");
                return response;
            }
        }
        if (response.length() > 0 && (millis() - lastByteTime) >= idleTimeoutMs) {
            break;
        }
        delay(1);
    }
    hostCom.println(" CIE response timeout/incomplete");
    return response;
}

// Fast fixed-width decimal parsing for the RTIM timestamp. No validation:
// the caller has already checked CRC and length.
static inline int parse2(const char* p) {
    return (p[0]-'0')*10 + (p[1]-'0');
}
static inline int parse4(const char* p) {
    return (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
}

// Validate and strip the framing of an ASCII reply:
//     <data ...><CHK hi><CHK lo><EOT>
// The CRC is recomputed over <data> and compared with the received hex digits.
String ServoCIEData::parseASCIIResponse(const char* response, size_t len, bool& crcError) {
    crcError = true;
    if (len < 4) { // at least 1 byte data + 2 CRC + 1 EOT
        hostCom.println("Response too short");
        return "";
    }
    if (response[len - 1] != EOT) {
        hostCom.println("Response missing EOT");
        return "";
    }
    // The two characters before EOT are the CRC in ASCII hex.
    char receivedCRCstr[3] = { response[len - 3], response[len - 2], '\0' };
    uint8_t receivedCRC = (uint8_t)strtol(receivedCRCstr, nullptr, 16);

    // Data portion: everything except the last 3 bytes (CRC + EOT).
    size_t dataLen = len - 3;
    char data[dataLen + 1];
    memcpy(data, response, dataLen);
    data[dataLen] = '\0';

    uint8_t computedCRC = CRC_calc(data);
    if (computedCRC != receivedCRC) {
        hostCom.printf("CRC mismatch! expected %02X, got %s\n", computedCRC, receivedCRCstr);
        return "";
    }

    crcError = false;
    return String(data, dataLen);
}

// For replies whose payload ends with a status digit, e.g. RCTY "SERVO-U0":
// '0' = OK, '1' = internal communication error in the ventilator.
String ServoCIEData::parseASCIIStatusResponse(const char* response, size_t len, bool& crcError, bool& statusError) {
    String data = parseASCIIResponse(response, len, crcError);
    statusError = false;
    if (crcError || data.length() == 0) {
        return "";
    }

    char statusChar = data[data.length() - 1];
    statusError = (statusChar == '1');
    data.remove(data.length() - 1);
    return data;
}

// RTIM reply payload: "YYYYMMDDhhmmss" (14 digits, ventilator local time).
// Also sets the ESP32 system clock, which is the logger's only time source.
DateTime ServoCIEData::parseRTIMResponse(const char* response, size_t len) {
    bool crcError = false;
    String timeData = parseASCIIResponse(response, len, crcError);
    if (crcError || timeData.length() != 14) return DateTime(); // invalid

    int year   = parse4(timeData.c_str() + 0);
    int month  = parse2(timeData.c_str() + 4);
    int day    = parse2(timeData.c_str() + 6);
    int hour   = parse2(timeData.c_str() + 8);
    int minute = parse2(timeData.c_str() +10);
    int second = parse2(timeData.c_str() +12);

    DateTime dt(year, month, day, hour, minute, second, true);
    hostCom.printf("SERVO time: %s\n", dt.toString().c_str());
    dt.setRTC();
    return dt;
}

// RCTY reply payload: "<device type><status digit>", e.g. "SERVO-U0".
// On CRC error the previous servoID is kept.
void ServoCIEData::parseRCTYResponse(const char* response, size_t len) {
    bool crcError = false;
    bool statusError = false;
    String parsedServoID = parseASCIIStatusResponse(response, len, crcError, statusError);
    if (!crcError) {
        servoID = parsedServoID;
    }

    if (statusError) hostCom.println("Device reported internal communication error");

    hostCom.printf("servoID = %s\n", servoID.c_str());
}

// RSEN reply payload: the serial number. On CRC error the previous value is kept.
void ServoCIEData::parseRSENResponse(const char* response, size_t len) {
    bool crcError = false;
    String parsedServoSN = parseASCIIResponse(response, len, crcError);
    if (!crcError) {
        servoSN = parsedServoSN;
    }
    hostCom.printf("servo S/N = %s\n", servoSN.c_str());
}

// Device type immediately followed by serial number; used in the log headers.
String ServoCIEData::getServoID() {
    return servoID + servoSN;
}


// =============================================================================
// 4. Link check and init sequence
// =============================================================================

// SCI "empty command" used as a ping:
//   send    <EOT>
//   expect  '*' '2' 'A' <EOT>
// '*' is the acknowledgement and "2A" is its checksum (XOR of the single
// character '*' = 0x2A) in ASCII hex.
// Before pinging, ESC stops any ongoing stream and the receive buffer is
// flushed, so streamed data is not mistaken for the reply.
bool ServoCIEData::CIE_comCheck() {
    const unsigned long TIMEOUT_MS = 500;   // local reply timeout; shadows the global 8 s link timeout
    unsigned long start = millis();

    hostCom.println("DEBUG sending esc");
    servoCom.write(ESC);

    hostCom.println("Clearing buffer");

    delay(200);   // let the ventilator finish whatever it was sending

    hostCom.printf("DEBUG Flushed after ESC ");
    while (servoCom.available() > 0) {
        char b = servoCom.read();
        hostCom.printf("0x%02X ", b);
        hostCom.print(b);
        hostCom.print("\t");
    }
    servoCom.write(EOT);
    hostCom.print("\nChecking CIE connection, ");

    // Wait for the 4-byte reply: '*', CHK hi, CHK lo, EOT.
    // Note: `start` was taken before the 200 ms delay above, so about 300 ms
    // remain for the reply.
    while (servoCom.available() < 4 && (millis() - start) < TIMEOUT_MS) {
        delay(5);
    }

    if (servoCom.available() < 4) {
        return false; // Timeout or incomplete response
    }

    uint8_t receivedStar = servoCom.read();   // should be '*'
    uint8_t receivedCHK1 = servoCom.read();   // should be '2'
    uint8_t receivedCHK2 = servoCom.read();   // should be 'A'
    uint8_t receivedEOT  = servoCom.read();   // should be EOT

    hostCom.printf(" Received: STAR=0x%02X, CHK1=0x%02X, CHK2=0x%02X, EOT=0x%02X\n",
                receivedStar, receivedCHK1, receivedCHK2, receivedEOT);

    uint8_t crc = '*';  // XOR of a single byte is the byte itself

    char crcStr[3];
    snprintf(crcStr, sizeof(crcStr), "%02X", crc);

    return (receivedStar == '*') &&
           (receivedCHK1 == crcStr[0]) &&
           (receivedCHK2 == crcStr[1]) &&
           (receivedEOT == EOT);
}

void ServoCIEData::setComOpen(bool state) {
    comOpen = state;
}

bool ServoCIEData::isComOpen() const {
    return comOpen;
}

void ServoCIEData::setLastInitAttempt(unsigned long timestamp) {
    lastInitAttempt = timestamp;
}

unsigned long ServoCIEData::getLastInitAttempt() const {
    return lastInitAttempt;
}

void ServoCIEData::setLastMessageTime(unsigned long timestamp) {
    lastMessageTime = timestamp;
}

unsigned long ServoCIEData::getLastMessageTime() const {
    return lastMessageTime;
}

// Full init sequence. Blocking: each command waits up to 500 ms for its reply.
// See the file header of ServoCIEData.hpp for the purpose of each command.
bool ServoCIEData::CIE_setup() {
    // Build every command string up front, so a config with too many channels
    // is caught before anything is sent.
    // Every SDAD* payload is the channel list of its config file, in file
    // order, which is also the order of the values in the stream.
    // RCCO "102": read the gain/offset configuration of channel 102 (diagnostic).
    if (!buildCommand(CMD_RTIM, sizeof(CMD_RTIM), "RTIM", "") ||
        !buildCommand(CMD_RCTY, sizeof(CMD_RCTY), "RCTY", "") ||
        !buildCommand(CMD_RSEN, sizeof(CMD_RSEN), "RSEN", "") ||
        !buildCommand(CMD_RHVE, sizeof(CMD_RHVE), "RHVE", "") ||
        !buildCommand(CMD_SPVE, sizeof(CMD_SPVE), "SPVE", "") ||
        !buildCommand(CMD_SDADB, sizeof(CMD_SDADB), "SDADB", concatConfigChannels(metrics, metricCount)) ||
        !buildCommand(CMD_SDADS, sizeof(CMD_SDADS), "SDADS", concatConfigChannels(settings, settingCount)) ||
        !buildCommand(CMD_SDADA, sizeof(CMD_SDADA), "SDADA", concatConfigChannels(alarms, alarmCount)) ||
        !buildCommand(CMD_SDADC, sizeof(CMD_SDADC), "SDADC", concatConfigChannels(curves, curveCount)) ||
        !buildCommand(CMD_RCCO, sizeof(CMD_RCCO), "RCCO", "102") ||
        !buildCommand(CMD_RDAD, sizeof(CMD_RDAD), "RDAD", "") ||
        !buildCommand(CMD_RADAB, sizeof(CMD_RADAB), "RADAB", "") ||
        !buildCommand(CMD_RADAS, sizeof(CMD_RADAS), "RADAS", "") ||
        !buildCommand(CMD_RADC, sizeof(CMD_RADC), "RADC", "")) {
        setComOpen(false);
        return false;
    }

    if (CIE_comCheck()) {
        hostCom.println("CIE communication OK");
        setComOpen(true);
        setLastInitAttempt(millis());
        setLastMessageTime(millis());

    } else {
        hostCom.println("CIE communication FAILED");
        setComOpen(false);
        setLastInitAttempt(millis());
        return false;
    }

    // Ventilator clock -> ESP32 system clock. An invalid reply leaves the clock unchanged.
    Send_SERVO_CMD_ASCII(CMD_RTIM);
    String rtimResponse = getCIEResponse();
    DateTime dt = parseRTIMResponse(rtimResponse.c_str(), rtimResponse.length());
    DateTime::setRTC(dt); // only sets the clock if dt is valid

    // Device type. RCTY also clears the subscription table and resets the
    // protocol version to 0001, so it must come before SPVE and SDAD*.
    Send_SERVO_CMD_ASCII(CMD_RCTY);
    String rctyResponse = getCIEResponse();
    parseRCTYResponse(rctyResponse.c_str(),rctyResponse.length() );

    Send_SERVO_CMD_ASCII(CMD_RSEN);
    String rsenResponse = getCIEResponse();
    parseRSENResponse(rsenResponse.c_str(),rsenResponse.length() );

    // Read highest supported SCI protocol version from the ventilator and
    // switch to it. Without SPVE the ventilator stays on protocol 0001 and
    // rejects channels introduced later (e.g. ER1214 on SDADA).
    Send_SERVO_CMD_ASCII(CMD_RHVE);
    String rhveResponse = getCIEResponse();
    hostCom.print("[DBG] RHVE response: "); hostCom.println(rhveResponse);
    // The first 4 digits of the reply are the version, e.g. "0010".
    char ver[5] = {0};
    int vi = 0;
    for (size_t k = 0; k < rhveResponse.length() && vi < 4; ++k) {
        char c = rhveResponse[k];
        if (c >= '0' && c <= '9') ver[vi++] = c;
    }
    if (vi == 4) {
        // CMD_SPVE holds just "SPVE"; append the version, e.g. "SPVE0010".
        char spveCmd[16];
        if (buildCommand(spveCmd, sizeof(spveCmd), CMD_SPVE, ver)) {
            hostCom.print("[DBG] Sending SPVE: "); hostCom.println(spveCmd);
            Send_SERVO_CMD_ASCII(spveCmd);
            String spveResponse = getCIEResponse();
            hostCom.print("[DBG] SPVE response: "); hostCom.println(spveResponse);
        }
    } else {
        hostCom.println("[DBG] RHVE did not return a 4-digit version, skipping SPVE");
    }

    // Subscriptions. Each SDAD* replies "*<CHK><EOT>"; the reply is read and discarded.
    // Breath (metric) channels:
    Send_SERVO_CMD_ASCII(CMD_SDADB);
    getCIEResponse();

    // Settings channels:
    Send_SERVO_CMD_ASCII(CMD_SDADS);
    getCIEResponse();

    // Alarm channels:
    hostCom.print("[DBG] Sending SDADA: "); hostCom.println(CMD_SDADA);
    Send_SERVO_CMD_ASCII(CMD_SDADA);
    {
        String r = getCIEResponse();
        hostCom.print("[DBG] SDADA response: "); hostCom.println(r);
    }

    // Curve channels. The SCI refuses RADC (error BER16) when no curve channel
    // is subscribed, so CurveConfig.txt needs at least one active line.
    if (curveCount <= 0) {
        hostCom.println("⚠ No curve channels configured: RADC will be refused by the ventilator");
    }
    Send_SERVO_CMD_ASCII(CMD_SDADC);
    getCIEResponse();
    // The stream starts with absolute values for curve slot 0.
    CurveCounter = 0;
    for (int i = 0; i < MaxCurves; ++i) curveHasBase[i] = false;

    // Diagnostics only; the replies are echoed by getCIEResponse() and discarded.
    Send_SERVO_CMD_ASCII(CMD_RCCO);   // channel configuration of channel 102
    getCIEResponse();

    Send_SERVO_CMD_ASCII(CMD_RDAD);   // read back the subscription table
    getCIEResponse();

    Send_SERVO_CMD_ASCII(CMD_RADAB);  // one-shot read of current breath data
    getCIEResponse();

    Send_SERVO_CMD_ASCII(CMD_RADAS);  // one-shot read of current settings
    getCIEResponse();

    // Start continuous streaming. From here on the ventilator only reacts to
    // ESC (and PING); all bytes go to parseCIEData() via loop(). The read
    // below swallows the first part of the stream (up to 500 ms); the parser
    // resynchronises on the next frame.
    Send_SERVO_CMD_ASCII(CMD_RADC);

    getCIEResponse();
    hostCom.print("\nCIE setup complete");
    setLastMessageTime(millis());

    return true;
}


// =============================================================================
// 5. Channel configuration files
// =============================================================================

// Load order per file: SD (then copy to SPIFFS), else SPIFFS (then copy to SD
// as an editable template). Without an SD card: SPIFFS only.
// An SD file that exists but has no valid lines counts as missing, so a
// truncated SD file never overwrites the good SPIFFS copy.
void ServoCIEData::initializeConfigs(const char* metricPath, const char* settingPath, const char* curvePath, const char* alarmPath) {
    // The load flags are members, so other code can see what was loaded.
    metricConfigLoaded = false;
    settingConfigLoaded = false;
    curveConfigLoaded = false;
    alarmConfigLoaded = false;

    bool sdReady = sdManager && (sdManager->isCardPresent() || sdManager->begin());
    if (sdReady) {
        if (loadConfigFromSD(metricPath, metrics, metricCount, MaxMetrics)) {
            syncConfigSDToSPIFFS(metricPath);
            metricConfigLoaded = true;
        } else {
            hostCom.println("⚠ MetricConfig SD file missing, trying SPIFFS...");
            if (loadConfigFromSPIFFS(metricPath, metrics, metricCount, MaxMetrics)) {
                hostCom.println("✔ MetricConfig loaded from SPIFFS");
                syncConfigSPIFFSToSD(metricPath);
                metricConfigLoaded = true;
            }
        }

        if (loadConfigFromSD(settingPath, settings, settingCount, MaxSettings)) {
            syncConfigSDToSPIFFS(settingPath);
            settingConfigLoaded = true;
        } else {
            hostCom.println("⚠ SettingConfig SD file missing, trying SPIFFS...");
            if (loadConfigFromSPIFFS(settingPath, settings, settingCount, MaxSettings)) {
                hostCom.println("✔ SettingConfig loaded from SPIFFS");
                syncConfigSPIFFSToSD(settingPath);
                settingConfigLoaded = true;
            }
        }
        if (loadConfigFromSD(curvePath, curves, curveCount, MaxCurves)) {
            syncConfigSDToSPIFFS(curvePath);
            curveConfigLoaded = true;
        } else {
            hostCom.println("⚠ CurveConfig SD file missing, trying SPIFFS...");
            if (loadConfigFromSPIFFS(curvePath, curves, curveCount, MaxCurves)) {
                hostCom.println("✔ CurveConfig loaded from SPIFFS");
                syncConfigSPIFFSToSD(curvePath);
                curveConfigLoaded = true;
            }
        }

        if (alarmPath) {
            if (loadConfigFromSD(alarmPath, alarms, alarmCount, MaxAlarms)) {
                syncConfigSDToSPIFFS(alarmPath);
                alarmConfigLoaded = true;
            } else {
                hostCom.println("⚠ AlarmConfig SD file missing, trying SPIFFS...");
                if (loadConfigFromSPIFFS(alarmPath, alarms, alarmCount, MaxAlarms)) {
                    hostCom.println("✔ AlarmConfig loaded from SPIFFS");
                    syncConfigSPIFFSToSD(alarmPath);
                    alarmConfigLoaded = true;
                }
            }
        }
    } else {
        hostCom.println("⚠ SD mount failed, using SPIFFS only...");
        if (SPIFFS.begin(false)) {
            metricConfigLoaded = loadConfigFromSPIFFS(metricPath, metrics, metricCount, MaxMetrics);
            settingConfigLoaded = loadConfigFromSPIFFS(settingPath, settings, settingCount, MaxSettings);
            curveConfigLoaded = loadConfigFromSPIFFS(curvePath, curves, curveCount, MaxCurves);
            if (alarmPath) {
                alarmConfigLoaded = loadConfigFromSPIFFS(alarmPath, alarms, alarmCount, MaxAlarms);
            }
        }
    }

    // Echo what was loaded, so the boot log shows the active channel set.
    if (metricConfigLoaded) {
        printAllConfigs(metrics, metricCount, metricPath);
     } else {
        hostCom.println("❌ No metric configs loaded.");
    }

    if (settingConfigLoaded) {
        printAllConfigs(settings, settingCount, settingPath);
    } else {
        hostCom.println("❌ No setting configs loaded.");
    }

    if (curveConfigLoaded) {
        printAllConfigs(curves, curveCount, curvePath);
    } else {
        hostCom.println("❌ No curve configs loaded.");
    }

    if (alarmConfigLoaded) {
        printAllConfigs(alarms, alarmCount, alarmPath);
    } else {
        hostCom.println("❌ No alarm configs loaded.");
    }
}

uint8_t ServoCIEData:: getBreathPhase() {
    return breathPhase;
}

// Channel numbers of all active entries, concatenated: "113" "114" -> "113114".
// The SCI expects exactly 3 digits per channel, so channel strings in the
// config files must be zero-padded ("000", not "0").
String ServoCIEData::concatConfigChannels(const Configs configs[], int size) {
    String result = "";
    for (int i = 0; i < size; ++i) {
        if (configs[i].active) {
            result += configs[i].channel;
        }
    }
    return result;
}

bool ServoCIEData::buildCommand(char* outBuffer, size_t outSize, const char* prefix, const String& payload) {
    int written = snprintf(outBuffer, outSize, "%s%s", prefix, payload.c_str());
    if (written < 0 || (size_t)written >= outSize) {
        hostCom.printf("Command too long: %s payload length %u exceeds buffer %u\n",
                       prefix, (unsigned int)payload.length(), (unsigned int)outSize);
        if (outSize > 0) outBuffer[0] = '\0';
        return false;
    }
    return true;
}

// Read a config file line by line. Blank lines, '#' comments, unparseable
// lines and lines marked inactive are skipped; entries beyond maxConfigs are
// dropped silently. Returns true only if at least one entry was stored.
bool ServoCIEData::loadConfigFromSD(const char* path, Configs* configs, int& configCount, const int maxConfigs) {
    configCount = 0;
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("❌ SD " + String(path) + " file not found");
        return false;
    }

    hostCom.println("✅ Loaded " + String(path) + " config from SD");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs m;
        if (parseConfigLine(line, m) && m.active && configCount < maxConfigs) {
            configs[configCount++] = m;
        }
    }

    file.close();
    if (configCount == 0) {
        hostCom.println("⚠ SD " + String(path) + " had no valid entries");
        return false;
    }
    return true;
}

// Identical to loadConfigFromSD(), reading from SPIFFS.
bool ServoCIEData::loadConfigFromSPIFFS(const char* path, Configs* configs, int& configCount, const int maxConfigs) {
    configCount = 0;
    File file = SPIFFS.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("? SPIFFS " + String(path) + " file not found");

        return false;
    }

    hostCom.println("✅ Loaded " + String(path) + " config from SPIFFS");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs m;
        if (parseConfigLine(line, m) && m.active && configCount < maxConfigs) {
            configs[configCount++] = m;
        }
    }

    file.close();
    if (configCount == 0) {
        hostCom.println("⚠ SPIFFS " + String(path) + " had no valid entries");
        return false;
    }
    return true;
}

// Copy SD -> SPIFFS byte by byte. FILE_WRITE truncates the target first.
// Keeps the SPIFFS fallback in step with the operator's edits on the card.
bool ServoCIEData::syncConfigSDToSPIFFS(const char* path) {
    File inFile = SD.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SD " + String(path) +" file not found for syncing");
        return false;
    }

    File outFile = SPIFFS.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SPIFFS " + String(path) +" file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SD → SPIFFS " + String(path));
    return true;
}

// Copy SPIFFS -> SD byte by byte. Used to put a template on a fresh card.
bool ServoCIEData::syncConfigSPIFFSToSD(const char* path) {
    File inFile = SPIFFS.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SPIFFS " + String(path) +" file not found for syncing");
        return false;
    }

    File outFile = SD.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SD " + String(path) +" file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SPIFFS → SD (" + String(path) +")");
    return true;
}

// "<channel>\t<label>\t<unit>\t<scale>\t<offset>[\t<active>]"
// With 5 fields the entry is active. With 6, the last field must be "true" or
// "1" to be active. Numbers that fail to parse become 0 (String::toFloat()).
bool ServoCIEData::parseConfigLine(const String& line, Configs& m) {
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    int tab3 = line.indexOf('\t', tab2 + 1);
    int tab4 = line.indexOf('\t', tab3 + 1);
    int tab5 = line.indexOf('\t', tab4 + 1); // optional sixth field

    if (tab1 == -1 || tab2 == -1 || tab3 == -1 || tab4 == -1) return false;

    m.channel     = line.substring(0, tab1);
    m.label       = line.substring(tab1 + 1, tab2);
    m.unit        = line.substring(tab2 + 1, tab3);
    m.scaleFactor = line.substring(tab3 + 1, tab4).toFloat();

    if (tab5 != -1) {
        m.offset = line.substring(tab4 + 1, tab5).toFloat();
        String activeStr = line.substring(tab5 + 1);
        activeStr.trim();
        m.active = (activeStr == "true" || activeStr == "1");
    } else {
        m.offset = line.substring(tab4 + 1).toFloat();
        m.active = true; // default
    }

    return true;
}


// =============================================================================
// 6. Formatting helpers
// =============================================================================

// Debug print of one config table. (The final count line says "curves" for
// every table type.)
void ServoCIEData::printAllConfigs(const Configs* configs, const int numConfigs, const char* ConfigPaths){
 hostCom.printf("\n%s configuration:\n", ConfigPaths);
    for (int i = 0; i < numConfigs; i++) {
        hostCom.printf("%s:\t%s [%s]\tScale: %.4f\tOffset: %.2f %s\n",
                      configs[i].channel.c_str(),
                      configs[i].label.c_str(),
                      configs[i].unit.c_str(),
                      configs[i].scaleFactor,
                      configs[i].offset,
                      configs[i].active ? "true" : "false");
    }
    hostCom.printf("Total num curves: %d\n", numConfigs);
}


// Unused. Note the sign: this ADDS the offset, while the frame handlers
// subtract it (scaled = raw * scale - offset).
void ServoCIEData::scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray) {
    for (int i = 0; i < count; ++i) {
        scaledArray[i] = unscaledArray[i] * configsArray[i].scaleFactor + configsArray[i].offset;
    }
};


// Tab-separated scaled values (2 decimals) followed by '\n'. A raw value of
// 0x7EFF (32511, "no valid data") is written as the text "No Valid Data".
// With includeHeader, a tab-separated label line is prepended.
String ServoCIEData::getScaledValuesAsString(const Configs* configsArray, int count, bool includeHeader) {
    String values = "";
    String header = "";
    for (int i = 0; i < count; i++) {
        header += configsArray[i].label;
        if ((int)configsArray[i].unscaled == 32511) {
            values += "No Valid Data";
        } else {
            values += String(configsArray[i].scaled, 2);
        }
        // Add delimiter except for last item
        if (i < count - 1) {
            header += '\t';
            values += '\t';
        }
    }
    if (includeHeader) {
        return header + "\n" + values + "\n";
    } else {
        return values + "\n";
    }
}

// The three helpers below join one field of each entry with '\t' and end
// the line with '\n'.
String ServoCIEData::getChannelsAsString(const Configs* configsArray, int count) {
    String result = "";
    for (int i = 0; i < count; ++i) {
        result += configsArray[i].channel;
        result += (i < count - 1) ? '\t' : '\n';
    }
    return result;
}

String ServoCIEData::getLabelsAsString(const Configs* configsArray, int count) {
    String result = "";
    for (int i = 0; i < count; ++i) {
        result += configsArray[i].label;
        result += (i < count - 1) ? '\t' : '\n';
    }
    return result;
}

String ServoCIEData::getUnitsAsString(const Configs* configsArray, int count) {
    String result = "";
    for (int i = 0; i < count; ++i) {
        result += configsArray[i].unit;
        result += (i < count - 1) ? '\t' : '\n';
    }
    return result;
}

String ServoCIEData::getAlarmLabelsAsString() {
    String result = "";
    for (int i = 0; i < alarmCount; ++i) {
        result += alarms[i].label;
        result += (i < alarmCount - 1) ? '\t' : '\n';
    }
    return result;
}

// One field per alarm channel, tab-separated:
//   "prio:status"  prio 0=undefined, 1=low, 2=medium, 3=high
//                  status 0=no alarm, 1=active, 2=silenced
//   "N/A"          alarm not applicable in the current ventilation mode
String ServoCIEData::getAlarmValuesAsString() {
    String result = "";
    for (int i = 0; i < alarmCount; ++i) {
        uint16_t val = alarms[i].unscaled;
        if (val == cieDataInvalid) {
            result += "N/A";
        } else {
            result += String((int)alarms[i].scaled) + ":" + String(val);
        }
        result += (i < alarmCount - 1) ? '\t' : '\n';
    }
    return result;
}

// Status 1 (active) or 2 (silenced) on any configured alarm channel.
bool ServoCIEData::hasActiveAlarm() const {
    for (int i = 0; i < alarmCount; ++i) {
        if (alarms[i].unscaled == 1 || alarms[i].unscaled == 2) return true;
    }
    return false;
}
