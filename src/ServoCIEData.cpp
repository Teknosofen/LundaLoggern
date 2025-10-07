#include "ServoCIEData.hpp"
#include "DateTime.hpp"
#include "SDManager.hpp"

ServoCIEData::ServoCIEData(SDManager* manager) 
    : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0), sdManager(manager) {
// ServoCIEData::ServoCIEData() : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0) {
    // Initialize other members if needed
}

bool ServoCIEData::begin() {
    hostCom.println("ServoCIEData begin called");

    servoCom.write(ESC); // stop any ongoing communication
    hostCom.printf("Flushed after ESC: ");
    while (servoCom.available() > 0) {
        uint8_t b = servoCom.read();
        // hostCom.printf(" %s 0x%02X ", b, b);
    }
    hostCom.println("");
    CIE_setup();
    return true;
}

// ===============================
// Dispatcher
// ===============================
void ServoCIEData::parseCIEData(char NextSCI_chr) {
    switch (RunMode) {
        case Awaiting_Info:   handleAwaitingInfo(NextSCI_chr);   break;
        case End_Flag_Found:  RunMode = Awaiting_Info;           break;
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

// ===============================
// Handlers
// ===============================
void ServoCIEData::handleAwaitingInfo(uint8_t NextSCI_chr) {
    switch (NextSCI_chr) {
        case 'S':  RunMode = Settings_Data; settingsNo = 0; ByteCount = 2; break;
        case 'B':  RunMode = Breath_Data;   MetricNo = 0;  ByteCount = 2; break;
        case 'T':  RunMode = Trend_Data;                                break;
        case 'A':  RunMode = Alarm_Data;                                break;
        case 0xE0: RunMode = Error_Data;    ByteCount = 1;               break;
        case 0x80: RunMode = Value_Data;    ByteCount = 2;               break;
        case 0x81: RunMode = Phase_Data;    ByteCount = 1;               break;
        case 0x7F: RunMode = Awaiting_Info;                             break;
        default:   break;
    }
}

void ServoCIEData::handleBreathData(uint8_t NextSCI_chr) {
    if (ByteCount == 2) {
        metrics[MetricNo].unscaled = 256 * NextSCI_chr;
        --ByteCount;
    } else if (ByteCount == 1) {
        metrics[MetricNo].unscaled += NextSCI_chr;
        --ByteCount;
        metrics[MetricNo].scaled = metrics[MetricNo].unscaled * metrics[MetricNo].scaleFactor - metrics[MetricNo].offset;
    } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        String newFilename, dataStr;
        bool isNewFile = sdManager->updateFileNameIfChanged('M', newFilename);
        if (isNewFile) {
            dataStr = "Data from: " + getServoID() + "\n";
            dataStr += "Timestamp\t" + getLabelsAsString(metrics, MetricNo) + "\n";
            sdManager->appendData(dataStr, 'M');
            hostCom.print(dataStr);
        }
        dataStr = dateTime.getRTC().toString() + "\t" + getScaledValuesAsString(metrics, MetricNo);
        sdManager->appendData(dataStr, 'M');
        setLastMessageTime(millis());
        hostCom.print(dataStr);
        RunMode = End_Flag_Found;
    } else {
        MetricNo++;
        metrics[MetricNo].unscaled = 256 * NextSCI_chr;
        ByteCount = 1;
    }
}

void ServoCIEData::handleSettingsData(uint8_t NextSCI_chr) {
    if (ByteCount == 2) {
        settings[settingsNo].unscaled = 256 * NextSCI_chr;
        --ByteCount;
    } else if (ByteCount == 1) {
        settings[settingsNo].unscaled += NextSCI_chr;
        --ByteCount;
        settings[settingsNo].scaled = settings[settingsNo].unscaled * settings[settingsNo].scaleFactor - settings[settingsNo].offset;
    } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        String newFilename, dataStr;
        bool isNewFile = sdManager->updateFileNameIfChanged('S', newFilename);
        if (isNewFile) {
            dataStr = "Data from: " + getServoID() + "\n";
            dataStr += "Timestamp\t" + getLabelsAsString(settings, settingsNo) + "\n";
            sdManager->appendData(dataStr, 'S');
            hostCom.print(dataStr);
        }
        dataStr = dateTime.getRTC().toString() + "\t" + getScaledValuesAsString(settings, settingsNo, true);
        sdManager->appendData(dataStr, 'S');
        setLastMessageTime(millis());
        hostCom.print(dataStr);
        RunMode = End_Flag_Found;
    } else {
        settingsNo++;
        settings[settingsNo].unscaled = 256 * NextSCI_chr;
        ByteCount = 1;
    }
}

void ServoCIEData::handleValueData(uint8_t NextSCI_chr) {
    switch (CurveCounter) {
        case 0:
            if (ByteCount == 2) { cieCurve1 = 256 * NextSCI_chr; --ByteCount; }
            else if (ByteCount == 1) {
                cieCurve1 += NextSCI_chr;
                curves[CurveCounter].unscaled = cieCurve1;
                curves[CurveCounter].scaled = cieCurve1 * curves[0].scaleFactor - curves[0].offset;
                RunMode = Run_Mode;
                --ByteCount;
                CurveCounter = (curveCount > 1) ? 1 : 0;
            }
            break;

        case 1:
            if (ByteCount == 2) { cieCurve2 = 256 * NextSCI_chr; --ByteCount; }
            else if (ByteCount == 1) {
                cieCurve2 += NextSCI_chr;
                curves[CurveCounter].unscaled = cieCurve2;
                curves[CurveCounter].scaled = cieCurve2 * curves[1].scaleFactor - curves[1].offset;
                RunMode = Run_Mode;
                --ByteCount;
                CurveCounter = (curveCount > 2) ? 2 : 0;
            }
            break;

        case 2:
            if (ByteCount == 2) { cieCurve3 = 256 * NextSCI_chr; --ByteCount; }
            else if (ByteCount == 1) {
                cieCurve3 += NextSCI_chr;
                curves[CurveCounter].unscaled = cieCurve3;
                curves[CurveCounter].scaled = cieCurve3 * curves[2].scaleFactor - curves[2].offset;
                RunMode = Run_Mode;
                --ByteCount;
                CurveCounter = (curveCount > 3) ? 3 : 0;
            }
            setLastMessageTime(millis());
            break;

        case 3:
            if (ByteCount == 2) { cieCurve4 = 256 * NextSCI_chr; --ByteCount; }
            else if (ByteCount == 1) {
                cieCurve4 += NextSCI_chr;
                curves[CurveCounter].unscaled = cieCurve4;
                curves[CurveCounter].scaled = cieCurve4 * curves[3].scaleFactor - curves[3].offset;
                RunMode = Run_Mode;
                --ByteCount;
                CurveCounter = 0;
            }
            break;

        default: break;
    }
}

void ServoCIEData::handlePhaseData(uint8_t NextSCI_chr) {
    breathPhase = NextSCI_chr;
    --ByteCount;
    RunMode = Run_Mode;
}

void ServoCIEData::handleTrendData(uint8_t NextSCI_chr) {
    if (NextSCI_chr == EndFlag) {
        RunMode = End_Flag_Found;
    }
}

void ServoCIEData::handleAlarmData(uint8_t NextSCI_chr) {
    if (NextSCI_chr == EndFlag) {
        RunMode = End_Flag_Found;
    }
}

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

void ServoCIEData::handleRunMode(uint8_t NextSCI_chr) {
    switch (NextSCI_chr) {
        case ValueFlag: RunMode = Value_Data; ByteCount = 2; break;
        case PhaseFlag: RunMode = Phase_Data; ByteCount = 1; break;
        case EndFlag:   RunMode = End_Flag_Found;            break;
        default:        break;
    }
}


// remoed use of String
uint8_t ServoCIEData::CRC_calc(const char* localstring) {
    char chk = 0;
    for (size_t i = 0; localstring[i] != '\0'; i++) {
        chk ^= localstring[i];
    }
    return chk;
}

// New CRC: returns 2 ASCII hex characters in output buffer
void ServoCIEData::CRC_calcASCII(const char* localstring, char* outHex) {
    uint8_t chk = CRC_calc(localstring);  // compute XOR of all bytes
    // Convert to 2-character ASCII hex string
    snprintf(outHex, 3, "%02X", chk);     // outHex[0..1] = hex chars, outHex[2] = '\0'
}

// removed use of String
void ServoCIEData::Send_SERVO_CMD(const char* InStr) {
    char CRC = CRC_calc(InStr);
    servoCom.print(InStr);
    if (CRC < 0x10) {
        servoCom.print("0");
    } else {
        servoCom.write(CRC);          // Send raw CRC byte
    }
    servoCom.write(EOT);          // Send EOT

    // hostCom.print(InStr);
    hostCom.printf("CRC for %s = 0x%02X\n", InStr, CRC);
}

// Send command with ASCII CRC (2 hex chars) + EOT
void ServoCIEData::Send_SERVO_CMD_ASCII(const char* InStr) {
    char crcStr[3];                  // 2 ASCII chars + null terminator
    CRC_calcASCII(InStr, crcStr);    // compute CRC and convert to ASCII

    // Send command string
    servoCom.print(InStr);

    // Send ASCII CRC characters
    servoCom.print(crcStr);

    // Send EOT
    servoCom.write(EOT);

    // Log
    hostCom.printf("Sent command: %s, CRC ASCII: %s\n", InStr, crcStr);
}

String ServoCIEData::getCIEResponse() {
    String response = "";
    delay(30);
    hostCom.print("CIE data: ");
    while (servoCom.available()) {
        inByte = servoCom.read();
        hostCom.print(inByte);
        hostCom.print(" (0x");
        hostCom.print(inByte, HEX);
        hostCom.print(") ");
        response += (char)inByte;  // Append character to response
        if (inByte == EOT) break;
        // response += (char)inByte;  // Append character to response
    }
    hostCom.println(" All CIE data received");
    return response;
}

// Assumes: CRC_calc(const char* nullTerminated) is available
//          EOT check is done by the reader (getCIEResponse).
// helper functions for parsing digits quickly
static inline int parse2(const char* p) {
    return (p[0]-'0')*10 + (p[1]-'0');
}
static inline int parse4(const char* p) {
    return (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
}

// Generic parser for ASCII response with 2-char CRC and EOT
// Returns empty string if invalid
String ServoCIEData::parseASCIIResponse(const char* response, size_t len, bool* statusError) {
    if (len < 4) { // at least 1 byte data + 2 CRC + 1 EOT
        hostCom.println("Response too short");
        if (statusError) *statusError = true;
        return "";
    }
    // Check EOT
    if (response[len - 1] != EOT) {
        hostCom.println("Response missing EOT");
        if (statusError) *statusError = true;
        return "";
    }
    // Extract CRC from last 3rd and 2nd bytes
    char receivedCRCstr[3] = { response[len - 3], response[len - 2], '\0' };
    uint8_t receivedCRC = (uint8_t)strtol(receivedCRCstr, nullptr, 16);

    // Data portion: everything except last 3 bytes (CRC + EOT)
    size_t dataLen = len - 3;
    char data[dataLen + 1];
    memcpy(data, response, dataLen);
    data[dataLen] = '\0';
    // hostCom.printf("Debug ascii data: %s\n", data);

    uint8_t computedCRC = CRC_calc(data);
    if (computedCRC != receivedCRC) {
        hostCom.printf("CRC mismatch! expected %02X, got %s\n", computedCRC, receivedCRCstr);
        if (statusError) *statusError = true;
        return "";
    }

    // Optional: check status for commands that include a status byte as last data char
    if (statusError && dataLen > 1) {
        char statusChar = data[dataLen - 1];
        *statusError = (statusChar == '1'); // true if error
    }

    // Return data string excluding last status char if applicable
    return String(data, dataLen); //  - (statusError ? 1 : 0));
}

// RTIM
DateTime ServoCIEData::parseRTIMResponse(const char* response, size_t len) {
    bool crcError = false;
    String timeData = parseASCIIResponse(response, len, &crcError);
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

// RCTY
void ServoCIEData::parseRCTYResponse(const char* response, size_t len) {
    bool statusError = false;
    servoID = parseASCIIResponse(response, len, &statusError);

    // If we used statusError flag, the last char in servoID is the status.
    // Replace it with a space.
    if (servoID.length() > 0) {
        servoID.setCharAt(servoID.length() - 1, ' ');
    }
    
    if (statusError) hostCom.println("Device reported internal communication error");
    
    hostCom.printf("servoID = %s\n", servoID.c_str());
}

// RSN
void ServoCIEData::parseRSENResponse(const char* response, size_t len) {
    bool dummyError = false;
    servoSN = parseASCIIResponse(response, len, &dummyError);
    hostCom.printf("servo S/N = %s\n", servoSN.c_str());
}

// SERVO serial number
String ServoCIEData::getServoID() {
    return servoID + servoSN;
}

bool ServoCIEData::CIE_comCheck() {
    // Send:   <EOT>
    // Expect: *<CHK><EOT>
    //   - '*'   = start marker (literal 0x2A)
    //   - <CHK> = XOR of '*' (so just '*')
    //   - <EOT> = echo of EOT

    const unsigned long TIMEOUT_MS = 500;
    unsigned long start = millis();

    // stop any ongoing command:
    hostCom.println("DEBUG sending esc");
    servoCom.write(ESC);

    // 🔹 Clear any stale bytes in receive buffer
    hostCom.println("Clearing buffer");
    
    delay(200);

    // flush out whatever came back after ESC
    hostCom.printf("DEBUG Flushed after ESC ");
    while (servoCom.available() > 0) {
        char b = servoCom.read();
        hostCom.printf("0x%02X ", b);
        hostCom.print(b);
        hostCom.print("\t");
    }
    servoCom.write(EOT);
    hostCom.print("\nChecking CIE connection, ");

    // Wait for 4 bytes (*, CHK, DATA, EOT)
    while (servoCom.available() < 4 && (millis() - start) < TIMEOUT_MS) {
        delay(5);
    }

    if (servoCom.available() < 4) {
        return false; // Timeout or incomplete response
    }

    uint8_t receivedStar = servoCom.read();   // should be '*'
    uint8_t receivedCHK1 = servoCom.read();
    uint8_t receivedCHK2 = servoCom.read();
    uint8_t receivedEOT  = servoCom.read();   // <EOT>

    hostCom.printf(" Received: STAR=0x%02X, CHK1=0x%02X, CHK2=0x%02X, EOT=0x%02X\n",
                receivedStar, receivedCHK1, receivedCHK2, receivedEOT);
                // CRC is calculated over the response start marker '*'
    
    uint8_t crc = '*';  // XOR of single byte = itself

    // Convert CRC to ASCII hex
    char crcStr[3];
    snprintf(crcStr, sizeof(crcStr), "%02X", crc);

    // Validate reply
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

bool ServoCIEData::CIE_setup() {
    strcpy(CMD_RTIM,  "RTIM");
    strcpy(CMD_RCTY,  "RCTY");
    strcpy(CMD_RSEN,  "RSEN");
    strcpy(CMD_SDADB, "SDADB");     // strcpy(PAYLOAD_SDADB, "113114117100102101103104109108107122105106128");
    strcat(CMD_SDADB, concatConfigChannels(metrics, metricCount).c_str());  // Append PAYLOAD_SDADB to CMD_SDADB
    strcpy(CMD_SDADS, "SDADS"); // strcpy(PAYLOAD_SDADS, "400405408414406420410409437419");
    strcat(CMD_SDADS, concatConfigChannels(settings, settingCount).c_str());  // Append PAYLOAD_SDADB to CMD_SDADB
    strcpy(CMD_SDADC, "SDADC");
    strcpy(PAYLOAD_SDADC, "000004001");
    strcat(CMD_SDADC, PAYLOAD_SDADC);  // Append PAYLOAD_SDADC to CMD_SDADC
    strcpy(CMD_RCCO,  "RCCO102");
    strcpy(CMD_RDAD,  "RDAD");
    strcpy(CMD_RADAB, "RADAB");
    strcpy(CMD_RADAS, "RADAS");
    strcpy(CMD_RADC,  "RADC");
    
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
    
    // get SERVO ventilator clock:
    Send_SERVO_CMD_ASCII(CMD_RTIM);
    String rtimResponse = getCIEResponse();
    DateTime dt = parseRTIMResponse(rtimResponse.c_str(), rtimResponse.length());
    DateTime::setRTC(dt); // Static call, only sets if valid
    // if dt is invalid, RTC is not changed

    Send_SERVO_CMD_ASCII(CMD_RCTY);
    String rctyResponse = getCIEResponse();
    parseRCTYResponse(rctyResponse.c_str(),rctyResponse.length() );

    Send_SERVO_CMD_ASCII(CMD_RSEN);
    String rsenResponse = getCIEResponse();
    parseRSENResponse(rsenResponse.c_str(),rsenResponse.length() );

    // breath to breats / Metrics channels
    Send_SERVO_CMD_ASCII(CMD_SDADB);
    getCIEResponse();

    // settings channels
    Send_SERVO_CMD_ASCII(CMD_SDADS);
    getCIEResponse();

    // Curve channels
    Send_SERVO_CMD_ASCII(CMD_SDADC);
    getCIEResponse();

    // read config
    Send_SERVO_CMD_ASCII(CMD_RCCO);
    getCIEResponse();

    // read config
    Send_SERVO_CMD_ASCII(CMD_RDAD);
    getCIEResponse();

    Send_SERVO_CMD_ASCII(CMD_RADAB);
    getCIEResponse();
    
    Send_SERVO_CMD_ASCII(CMD_RADAS);
    getCIEResponse();
    
    Send_SERVO_CMD_ASCII(CMD_RADC);
    
    getCIEResponse();
    hostCom.print("\nCIE setup complete");
    setLastMessageTime(millis());
    
    return true;
}

void ServoCIEData::initializeConfigs(const char* metricPath, const char* settingPath, const char* curvePath) {
    bool metricConfigLoaded = false;
    bool settingConfigLoaded = false;
    bool curveConfigLoaded = false;
    // metrics[0].configType = "Metrics";
    // settings[0].configType = "Settings";
    // curves[0].configType = "Curves";

    if (SD.begin()) {
        if (loadConfigFromSD(metricPath, metrics, metricCount, MaxMetrics)) {
            // hostCom.println("✔ MetricConfig loaded from SD");
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
            // hostCom.println("✔ SettingConfig loaded from SD");
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
            // hostCom.println("✔ CurveConfig loaded from SD");
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
    } else {
        hostCom.println("⚠ SD mount failed, using SPIFFS only...");
        if (begin()) {
            metricConfigLoaded = loadConfigFromSPIFFS(metricPath, metrics, metricCount, MaxMetrics);
            settingConfigLoaded = loadConfigFromSPIFFS(settingPath, settings, settingCount, MaxSettings);
            curveConfigLoaded = loadConfigFromSPIFFS(curvePath, curves, curveCount, MaxCurves);
        }
    }

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
}

uint8_t ServoCIEData:: getBreathPhase() {
    // hostCom.println("DEBUG - phase was checked");
    return breathPhase;
}
// -------------------- Config helpers --------------------

String ServoCIEData::concatConfigChannels(const Configs configs[], int size) {
    String result = "";
    for (int i = 0; i < size; ++i) {
        result += configs[i].channel;
    }
    return result;
}

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
        if (parseConfigLine(line, m) && configCount < MaxMetrics) {
            configs[configCount++] = m;
        }
    }

    file.close();
    return true;
}

bool ServoCIEData::loadConfigFromSPIFFS(const char* path, Configs* configs, int& configCount, const int maxConfigs) {
    configCount = 0;
    File file = SPIFFS.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("? SPIFFS " + String(path) + " file not found");

        return false;
    }

    hostCom.println("? Loaded " + String(path) + " config from SPIFFS");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs m;
        if (parseConfigLine(line, m) && configCount < MaxMetrics) {
            configs[configCount++] = m;
        }
    }

    file.close();
    return true;
}

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


void ServoCIEData::scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray) {
    for (int i = 0; i < count; ++i) {
        scaledArray[i] = unscaledArray[i] * configsArray[i].scaleFactor + configsArray[i].offset;
    }
};


String ServoCIEData::getScaledValuesAsString(const Configs* configsArray, int count, bool includeHeader) {
    String values = "";
    String header = "";
    for (int i = 0; i < count; i++) {
        header += configsArray[i].label;
        if ((int)configsArray[i].unscaled == 32511) {
            values += "No Valid Data";
        } else {
            values += String(configsArray[i].scaled, 2); // 2 decimal places
        }
        // Add delimiter except for last item
        if (i < count - 1) {
            header += '\t';
            values += '\t';
        }
        // values += (i < count - 1) ? '\t' : '\n';
    }
    if (includeHeader) {
        return header + "\n" + values + "\n";
    } else {
        return values + "\n";
    }
}

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

