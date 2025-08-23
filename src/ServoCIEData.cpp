#include "ServoCIEData.hpp"

ServoCIEData::ServoCIEData() 
    : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0) {
// ServoCIEData::ServoCIEData() : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0) {
    // Initialize other members if needed
}

bool ServoCIEData::begin() {
    hostCom.println("ServoCIEData begin called");
    // if (!SPIFFS.begin(true)) {
    //     hostCom.println("⚠️ SPIFFS Mount Failed");
    //     return false;
    // }
    // hostCom.println("✔️ SPIFFS Mounted Successfully");
    CIE_setup();
    return true;
}

void ServoCIEData::parseCIEData(char NextSCI_chr) {
    switch (RunMode) {
        case Awaiting_Info:
            switch (NextSCI_chr) {
                case 'S':
                    RunMode = Settings_Data;
                    settingsNo = 0;
                    ByteCount = 2;
                    break;
                case 'B':
                    RunMode = Breath_Data;
                    MetricNo = 0;
                    ByteCount = 2;
                    break;
                case 'T':
                    RunMode = Trend_Data;
                    break;
                case 'A':
                    RunMode = Alarm_Data;
                    break;
                case 0xE0:
                    RunMode = Error_Data;
                    ByteCount = 1;
                    break;
                case 0x80:
                    RunMode = Value_Data;
                    ByteCount = 2;
                    break;
                case 0x81:
                    RunMode = Phase_Data;
                    ByteCount = 1;
                    break;
                case 0x7F:
                    RunMode = Awaiting_Info;
                    break;
                default:
                    break;
            }
            break;

        case End_Flag_Found:
            RunMode = Awaiting_Info;
            break;

        // new 2025-08-23
        case Breath_Data:
            if (ByteCount == 2) {
                metrics[MetricNo].unscaled = 256 * NextSCI_chr;
                --ByteCount;
            } else if (ByteCount == 1) {
                metrics[MetricNo].unscaled += NextSCI_chr;
                --ByteCount;
                metrics[MetricNo].scaled = metrics[MetricNo].unscaled * metrics[MetricNo].scaleFactor - metrics[MetricNo].offset;
            } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
                // instead of copying into ventO2BreathData,
                // you directly use metrics[k].scaled everywhere in your code
                // Example:
                // float rr = metrics[3].scaled;   // cieRR
                // float ie = metrics[11].scaled;  // cieIE


                // Special cases no longer needed I assume
                
                // float cieRR = metrics[3].scaled;
                // float cieIE = metrics[11].scaled;
                // float cieTi2Ttot = metrics[14].scaled;

                // if (cieIE < (cieDataInvalid - 1) * 0.01) {
                //     metrics[metricCount].scaled = 60.0 / (cieRR * (1.0 + cieIE)); // calcExpTime
                //     metrics[metricCount+1].scaled = cieIE * 60.0 / (cieRR * (1.0 + cieIE)); // calcInspTime
                // } else {
                //     metrics[metricCount].scaled = (1 - cieTi2Ttot) * 60 / cieRR; // calcExpTime
                //     metrics[metricCount+1].scaled = cieTi2Ttot * 60 / cieRR;     // calcInspTime
                // }

                RunMode = End_Flag_Found;
            } else {
                MetricNo++;
                metrics[MetricNo].unscaled = 256 * NextSCI_chr;
                ByteCount = 1;
            }
            break;

        case Settings_Data:
            if (ByteCount == 2) {
                settings[settingsNo].unscaled = 256 * NextSCI_chr;
                --ByteCount;
            } else if (ByteCount == 1) {
                settings[settingsNo].unscaled += NextSCI_chr;
                --ByteCount;
                settings[settingsNo].scaled = settings[settingsNo].unscaled * settings[settingsNo].scaleFactor - settings[settingsNo].offset;
            } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
                // no more servoSettings struct — use settings[k].scaled directly
                // Example:
                // float setRespRate = settings[0].scaled;
                // float setFiO2     = settings[3].scaled;

                RunMode = End_Flag_Found;
            } else {
                settingsNo++;
                settings[settingsNo].unscaled = 256 * NextSCI_chr;
                ByteCount = 1;
            }
            break;


        // case Breath_Data:
        //     if (ByteCount == 2) {
        //         MetricUnscaled[MetricNo] = 256 * NextSCI_chr;
        //         --ByteCount;
        //     } else if (ByteCount == 1) {
        //         MetricUnscaled[MetricNo] += NextSCI_chr;
        //         --ByteCount;
        //         MetricScaled[MetricNo] = MetricUnscaled[MetricNo] * MetricScaleFactors[MetricNo] - MetricOffset[MetricNo];
        //     } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        //         int k = 0;
        //         ventO2BreathData.cieVtCO2   = MetricScaled[k++];
        //         ventO2BreathData.cieFetCO2  = MetricScaled[k++];
        //         ventO2BreathData.cieMVCO2   = MetricScaled[k++];
        //         ventO2BreathData.cieRR      = MetricScaled[k++];
        //         ventO2BreathData.cieVtInsp  = MetricScaled[k++];
        //         ventO2BreathData.cieVtExp   = MetricScaled[k++];
        //         ventO2BreathData.cieMVinsp  = MetricScaled[k++];
        //         ventO2BreathData.cieMVExp   = MetricScaled[k++];
        //         ventO2BreathData.cieFiO2    = MetricScaled[k++];
        //         ventO2BreathData.ciePEEP    = MetricScaled[k++];
        //         ventO2BreathData.ciePplat   = MetricScaled[k++];
        //         ventO2BreathData.cieIE      = MetricScaled[k++];
        //         ventO2BreathData.ciePpeak   = MetricScaled[k++];
        //         ventO2BreathData.Pmean      = MetricScaled[k++];
        //         ventO2BreathData.cieTi2Ttot = MetricScaled[k];

        //         if (ventO2BreathData.cieIE < (cieDataInvalid - 1) * 0.01) {
        //             ventO2BreathData.calcExpTime = 60.0 / (ventO2BreathData.cieRR * (1.0 + ventO2BreathData.cieIE));
        //             ventO2BreathData.calcInspTime = ventO2BreathData.cieIE * 60.0 / (ventO2BreathData.cieRR * (1.0 + ventO2BreathData.cieIE));
        //         } else {
        //             ventO2BreathData.calcExpTime = (1 - ventO2BreathData.cieTi2Ttot) * 60 / ventO2BreathData.cieRR;
        //             ventO2BreathData.calcInspTime = ventO2BreathData.cieTi2Ttot * 60 / ventO2BreathData.cieRR;
        //         }
        //         RunMode = End_Flag_Found;
        //     } else { // check when this is appliccable
        //         MetricNo++;
        //         MetricUnscaled[MetricNo] = 256 * NextSCI_chr;
        //         ByteCount = 1;
        //     }

        //     break;

        //     case Settings_Data:
        //     if (ByteCount == 2) {
        //         settingsUnscaled[settingsNo] = 256 * NextSCI_chr;
        //         --ByteCount;
        //     } else if (ByteCount == 1) {
        //         settingsUnscaled[settingsNo] += NextSCI_chr;
        //         --ByteCount;
        //         settingsScaled[settingsNo] = settingsUnscaled[settingsNo] * settingsScaleFactors[settingsNo] - settingsOffset[settingsNo];
        //     } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
        //         int k = 0;
        //         servoSettings.setRespRate   = settingsScaled[k++];
        //         servoSettings.setMinuteVol  = settingsScaled[k++];
        //         servoSettings.setPeep       = settingsScaled[k++];
        //         servoSettings.setFiO2       = settingsScaled[k++];
        //         servoSettings.setInspPress  = settingsScaled[k++];
        //         servoSettings.setVt         = settingsScaled[k++];
        //         servoSettings.setVentMode   = settingsScaled[k++];
        //         servoSettings.setPatRange   = settingsScaled[k++];
        //         servoSettings.setComplianceCompensationOn = settingsScaled[k++];
        //         servoSettings.setIERatio    = settingsScaled[k];
        //         RunMode = End_Flag_Found;
        //     } else {
        //         settingsNo++;
        //         settingsUnscaled[settingsNo] = 256 * NextSCI_chr;
        //         ByteCount = 1;
        //     }
        //     break;
        case Value_Data:
            switch (CurveCounter) {
                case 0:
                    if (ByteCount == 2) {
                        cieFlow = 256 * NextSCI_chr;
                        --ByteCount;
                    } else if (ByteCount == 1) {
                        cieFlow += NextSCI_chr;
                        ventO2CurveData.cieFlow = cieFlow * 0.25 - 4000.0;
                        RunMode = Run_Mode;
                        --ByteCount;
                        if (NumberOfCurves > 1) CurveCounter = 1;
                        else CurveCounter = 0;
                    }
                    break;
                case 1:
                    if (ByteCount == 2) {
                        cieFCO2 = 256 * NextSCI_chr;
                        --ByteCount;
                    } else if (ByteCount == 1) {
                        cieFCO2 += NextSCI_chr;
                        ventO2CurveData.cieCO2 = (cieFCO2 * 0.1);
                        RunMode = Run_Mode;
                        --ByteCount;
                        if (NumberOfCurves > 2) CurveCounter = 2;
                        else CurveCounter = 0;
                    }
                    break;
                case 2:
                    if (ByteCount == 2) {
                        ciePaw = 256 * NextSCI_chr;

                        // Something missing here - or not?
                    } else if (ByteCount == 1) {
                        ciePaw += NextSCI_chr;
                        ventO2CurveData.ciePaw = (ciePaw * 0.1);
                        RunMode = Run_Mode;
                        --ByteCount;
                        if (NumberOfCurves > 3) CurveCounter = 3;
                        else CurveCounter = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
        case Phase_Data:
            phase = NextSCI_chr;
            ventO2CurveData.ciePhase = phase;
            --ByteCount;
            RunMode = Run_Mode;
            break;
        case Trend_Data:
            while (NextSCI_chr != EndFlag) {}
            RunMode = End_Flag_Found;
            break;
        case Alarm_Data:
            while (NextSCI_chr != EndFlag) {}
            RunMode = End_Flag_Found;
            break;
        case Error_Data:
            if (NextSCI_chr == EndFlag) {
                RunMode = End_Flag_Found;
            } else {
                Error_info = NextSCI_chr;
            }
            break;
        case Run_Mode:
            switch (NextSCI_chr) {
                case ValueFlag:
                    RunMode = Value_Data;
                    ByteCount = 2;
                    break;
                case PhaseFlag:
                    RunMode = Phase_Data;
                    ByteCount = 1;
                    break;
                case EndFlag:
                    RunMode = End_Flag_Found;
                    break;
                default:
                    switch (CurveCounter) {
                        case 0:
                            cieFlow += int8_t(NextSCI_chr);
                            ventO2CurveData.cieFlow = cieFlow * 0.25 - 4000.0;
                            if (NumberOfCurves > 1) CurveCounter = 1;
                            else CurveCounter = 0;
                            break;
                        case 1:
                            cieFCO2 += int8_t(NextSCI_chr);
                            ventO2CurveData.cieCO2 = (cieFCO2 * 0.1);
                            if (NumberOfCurves > 2) CurveCounter = 2;
                            else CurveCounter = 0;
                            break;
                        case 2:
                            ciePaw += int8_t(NextSCI_chr);
                            ventO2CurveData.ciePaw = (ciePaw * 0.1);
                            if (NumberOfCurves > 3) CurveCounter = 3;
                            else CurveCounter = 0;
                            break;
                        default:
                            break;
                    }
                    break;
            }
            break;
        default:
            break;
    }
}

void ServoCIEData::ScaleMetrics() {
    for (int i = 0; i < NumMetrics; i++) {
        MetricScaled[i] = MetricUnscaled[i] * MetricScaleFactors[i] - MetricOffset[i];
        hostCom.print(MetricLabels[i]);
        hostCom.print(" = ");
        hostCom.print(MetricScaled[i]);
        hostCom.print(" ");
        hostCom.println(MetricUnits[i]);
    }
    hostCom.println();
}

// remoed use of String
char ServoCIEData::CRC_calc(const char* localstring) {
    char chk = 0;
    for (size_t i = 0; localstring[i] != '\0'; i++) {
        chk ^= localstring[i];
    }
    return chk;
}


// removed use of String
void ServoCIEData::Send_SERVO_CMD(const char* InStr) {
    char CRC = CRC_calc(InStr);
    servoCom.print(InStr);
    if (CRC < 0x10) {
        servoCom.print("0");
    }
    servoCom.write(CRC);          // Send raw CRC byte
    servoCom.write(EOT);          // Send EOT

    // hostCom.print(InStr);
    hostCom.printf("CRC for %s = 0x%02X\n", InStr, CRC);

}


void ServoCIEData::getCIEResponse() {
    delay(30);
    while (servoCom.available()) {
        hostCom.print(" trying to get some CIE data");
        inByte = servoCom.read();
        hostCom.print(' ');
        hostCom.print(inByte);
        hostCom.print(' ');
        hostCom.print(inByte, HEX);
        hostCom.print(' ');
        if (inByte == EOT) break;
    }
    hostCom.println(" no more CIE data available to receive");
}



// 2025-08-09:
void ServoCIEData::CIE_setup() {
    strcpy(CMD_RTIM, "RTIM");
    strcpy(CMD_RCTY, "RCTY");

    strcpy(CMD_SDADB, "SDADB");
    // strcpy(PAYLOAD_SDADB, "113114117100102101103104109108107122105106128");
    strcat(CMD_SDADB, concatConfigChannels(metrics, metricCount).c_str());  // Append PAYLOAD_SDADB to CMD_SDADB

    // strcpy(PAYLOAD_SDADS, "400405408414406420410409437419");
    strcpy(CMD_SDADS, "SDADS");
    strcat(CMD_SDADS, concatConfigChannels(settings, settingCount).c_str());  // Append PAYLOAD_SDADB to CMD_SDADB

    strcpy(PAYLOAD_SDADC, "000004001");
    strcpy(CMD_SDADC, "SDADC");
    strcat(CMD_SDADC, PAYLOAD_SDADC);  // Append PAYLOAD_SDADC to CMD_SDADC

    strcpy(CMD_RCCO, "RCCO102");
    strcpy(CMD_RDAD, "RDAD");
    strcpy(CMD_RADAB, "RADAB");
    strcpy(CMD_RADAS, "RADAS");
    strcpy(CMD_RADC, "RADC");

    servoCom.write(EOT);
    getCIEResponse();

    servoCom.write(ESC);
    getCIEResponse();

    Send_SERVO_CMD(CMD_RTIM);
    getCIEResponse();

    Send_SERVO_CMD(CMD_RCTY);
    getCIEResponse();

    Send_SERVO_CMD(CMD_SDADB);
    getCIEResponse();

    Send_SERVO_CMD(CMD_SDADS);
    getCIEResponse();

    Send_SERVO_CMD(CMD_SDADC);
    getCIEResponse();

    Send_SERVO_CMD(CMD_RCCO);
    getCIEResponse();

    Send_SERVO_CMD(CMD_RDAD);
    getCIEResponse();

    Send_SERVO_CMD(CMD_RADAB);
    getCIEResponse();
    
    Send_SERVO_CMD(CMD_RADAS);
    getCIEResponse();
    
    Send_SERVO_CMD(CMD_RADC);
    hostCom.println(" CIE setup almost complete");
    getCIEResponse();
    hostCom.print("\nCIE setup complete");
}

void ServoCIEData::initializeConfigs(const char* metricPath, const char* settingPath) {
    bool metricConfigLoaded = false;
    bool settingConfigLoaded = false;

    if (SD.begin()) {
        if (loadMetricFromSD(metricPath)) {
            // hostCom.println("✔ MetricConfig loaded from SD");
            syncMetricSDToSPIFFS(metricPath);
            metricConfigLoaded = true;
        } else {
            hostCom.println("⚠ MetricConfig SD file missing, trying SPIFFS...");
            if (loadMetricFromSPIFFS(metricPath)) {
                hostCom.println("✔ MetricConfig loaded from SPIFFS");
                syncMetricSPIFFSToSD(metricPath);
                metricConfigLoaded = true;
            }
        }

        if (loadSettingFromSD(settingPath)) {
            // hostCom.println("✔ SettingConfig loaded from SD");
            syncSettingSDToSPIFFS(settingPath);
            settingConfigLoaded = true;
        } else {
            hostCom.println("⚠ SettingConfig SD file missing, trying SPIFFS...");
            if (loadSettingFromSPIFFS(settingPath)) {
                hostCom.println("✔ SettingConfig loaded from SPIFFS");
                syncSettingSPIFFSToSD(settingPath);
                settingConfigLoaded = true;
            }
        }
    } else {
        hostCom.println("⚠ SD mount failed, using SPIFFS only...");
        if (begin()) {
            metricConfigLoaded = loadMetricFromSPIFFS(metricPath);
            settingConfigLoaded = loadSettingFromSPIFFS(settingPath);
        }
    }

    if (metricConfigLoaded) {
        printAllMetrics();
    } else {
        hostCom.println("❌ No metric configs loaded.");
    }

    if (settingConfigLoaded) {
        printAllSettings();
    } else {
        hostCom.println("❌ No setting configs loaded.");
    }
}

// -------------------- Config helpers --------------------

String ServoCIEData::concatConfigChannels(const Configs configs[], int size) {
    String result = "";
    for (int i = 0; i < size; ++i) {
        result += configs[i].channel;
    }
    return result;
}


// -------------------- Metric Config --------------------

bool ServoCIEData::loadMetricFromSD(const char* path) {
    metricCount = 0;
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("❌ SD metric file not found");
        metricConfigLoaded = false;
        return false;
    }

    hostCom.println("✅ Loaded metric config from SD");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs m;
        if (parseMetricLine(line, m) && metricCount < MaxMetrics) {
            metrics[metricCount++] = m;
        }
    }

    file.close();
    metricConfigLoaded = true;
    return true;
}

bool ServoCIEData::loadMetricFromSPIFFS(const char* path) {
    metricCount = 0;
    File file = SPIFFS.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("❌ SPIFFS metric file not found");
        metricConfigLoaded = false;
        return false;
    }

    hostCom.println("✅ Loaded metric config from SPIFFS");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs m;
        if (parseMetricLine(line, m) && metricCount < MaxMetrics) {
            metrics[metricCount++] = m;
        }
    }

    file.close();
    metricConfigLoaded = true;
    return true;
}

bool ServoCIEData::syncMetricSDToSPIFFS(const char* path) {
    File inFile = SD.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SD metric file not found for syncing");
        return false;
    }

    File outFile = SPIFFS.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SPIFFS metric file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SD → SPIFFS (metrics)");
    return true;
}

bool ServoCIEData::syncMetricSPIFFSToSD(const char* path) {
    File inFile = SPIFFS.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SPIFFS metric file not found for syncing");
        return false;
    }

    File outFile = SD.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SD metric file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SPIFFS → SD (metrics)");
    return true;
}

void ServoCIEData::printAllMetrics() {
    for (int i = 0; i < metricCount; i++) {
        hostCom.printf("%s:\t%s [%s]\tScale: %.4f\tOffset: %.2f\n",
                      metrics[i].channel.c_str(),
                      metrics[i].label.c_str(),
                      metrics[i].unit.c_str(),
                      metrics[i].scaleFactor,
                      metrics[i].offset);
    }
    hostCom.printf("Total num metrics: %d\n", metricCount);
}

bool ServoCIEData::parseMetricLine(const String& line, Configs& m) {
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    int tab3 = line.indexOf('\t', tab2 + 1);
    int tab4 = line.indexOf('\t', tab3 + 1);

    if (tab1 == -1 || tab2 == -1 || tab3 == -1 || tab4 == -1) return false;

    m.channel     = line.substring(0, tab1);
    m.label       = line.substring(tab1 + 1, tab2);
    m.unit        = line.substring(tab2 + 1, tab3);
    m.scaleFactor = line.substring(tab3 + 1, tab4).toFloat();
    m.offset      = line.substring(tab4 + 1).toFloat();

    return true;
}

// -------------------- Setting Config --------------------

bool ServoCIEData::loadSettingFromSD(const char* path) {
    settingCount = 0;
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("❌ SD setting file not found");
        settingConfigLoaded = false;
        return false;
    }

    hostCom.println("✅ Loaded settings from SD");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs s;
        if (parseSettingLine(line, s) && settingCount < MaxSettings) {
            settings[settingCount++] = s;
        }
    }

    file.close();
    settingConfigLoaded = true;
    return true;
}

bool ServoCIEData::loadSettingFromSPIFFS(const char* path) {
    settingCount = 0;
    File file = SPIFFS.open(path);
    if (!file || file.isDirectory()) {
        hostCom.println("❌ SPIFFS setting file not found");
        settingConfigLoaded = false;
        return false;
    }

    hostCom.println("✅ Loaded settings from SPIFFS");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        Configs s;
        if (parseSettingLine(line, s) && settingCount < MaxSettings) {
            settings[settingCount++] = s;
        }
    }

    file.close();
    settingConfigLoaded = true;
    return true;
}

bool ServoCIEData::syncSettingSDToSPIFFS(const char* path) {
    File inFile = SD.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SD setting file not found for syncing");
        return false;
    }

    File outFile = SPIFFS.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SPIFFS setting file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SD → SPIFFS (settings)");
    return true;
}

bool ServoCIEData::syncSettingSPIFFSToSD(const char* path) {
    File inFile = SPIFFS.open(path);
    if (!inFile || inFile.isDirectory()) {
        hostCom.println("❌ SPIFFS setting file not found for syncing");
        return false;
    }

    File outFile = SD.open(path, FILE_WRITE);
    if (!outFile) {
        hostCom.println("❌ Failed to open SD setting file for writing");
        inFile.close();
        return false;
    }

    while (inFile.available()) {
        outFile.write(inFile.read());
    }

    inFile.close();
    outFile.close();
    hostCom.println("🔁 Synced SPIFFS → SD (settings)");
    return true;
}

void ServoCIEData::printAllSettings() {
    for (int i = 0; i < settingCount; i++) {
        hostCom.printf("%s:\t%s [%s]\tScale: %.4f\tOffset: %.2f\n",
                      settings[i].channel.c_str(),
                      settings[i].label.c_str(),
                      settings[i].unit.c_str(),
                      settings[i].scaleFactor,
                      settings[i].offset);
    }
    hostCom.printf("Total num settings: %d\n", settingCount);
}

bool ServoCIEData::parseSettingLine(const String& line, Configs& s) {
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    int tab3 = line.indexOf('\t', tab2 + 1);
    int tab4 = line.indexOf('\t', tab3 + 1);

    if (tab1 == -1 || tab2 == -1 || tab3 == -1 || tab4 == -1) return false;

    s.channel     = line.substring(0, tab1);
    s.label       = line.substring(tab1 + 1, tab2);
    s.unit        = line.substring(tab2 + 1, tab3);
    s.scaleFactor = line.substring(tab3 + 1, tab4).toFloat();
    s.offset      = line.substring(tab4 + 1).toFloat();

    return true;
}

    // Method to scale both metrics and settings
void ServoCIEData::scaleAll(int metricSize, int settingSize) {
    // Scale metrics
    for (int i = 0; i < metricSize && i < metricCount; ++i) {
        MetricScaled[i] = MetricUnscaled[i] * metrics[i].scaleFactor + metrics[i].offset;
    }

    // Scale settings
    for (int i = 0; i < settingSize && i < settingCount; ++i) {
        settingsScaled[i] = settingsUnscaled[i] * settings[i].scaleFactor + settings[i].offset;
    }
}

void ServoCIEData::scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray) {
    for (int i = 0; i < count; ++i) {
        scaledArray[i] = unscaledArray[i] * configsArray[i].scaleFactor + configsArray[i].offset;
    }
};


String ServoCIEData::getScaledValuesAsString(const float* scaledArray, int count) {
    String result = "";
    for (int i = 0; i < count; ++i) {
        result += String(scaledArray[i], 4); // 4 decimal places
        result += (i < count - 1) ? '\t' : '\n';
    }
    return result;
}

// String metricsStr = getScaledValuesAsString(MetricScaled, metricCount);
// String settingsStr = getScaledValuesAsString(SettingsScaled, settingCount);

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

// String metricsLabels = getLabelsAsString(metrics, metricCount);
// String metricsUnits = getUnitsAsString(metrics, metricCount);
// String settingsLabels = getLabelsAsString(settings, settingCount);
// String settingsUnits = getUnitsAsString(settings, settingCount);
