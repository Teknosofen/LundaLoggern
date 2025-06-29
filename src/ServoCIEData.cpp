#include "ServoCIEData.hpp"

ServoCIEData::ServoCIEData() 
    : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0) {
// ServoCIEData::ServoCIEData() : RunMode(Awaiting_Info), ByteCount(0), MetricNo(0), settingsNo(0) {
    // Initialize other members if needed
}

void ServoCIEData::begin() {
    CIE_setup();
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

        case Breath_Data:
            if (ByteCount == 2) {
                MetricUnscaled[MetricNo] = 256 * NextSCI_chr;
                --ByteCount;
            } else if (ByteCount == 1) {
                MetricUnscaled[MetricNo] += NextSCI_chr;
                --ByteCount;
                MetricScaled[MetricNo] = MetricUnscaled[MetricNo] * MetricScaleFactors[MetricNo] - MetricOffset[MetricNo];
            } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
                int k = 0;
                ventO2BreathData.cieVtCO2 = MetricScaled[k++];
                ventO2BreathData.cieFetCO2 = MetricScaled[k++];
                ventO2BreathData.cieMVCO2 = MetricScaled[k++];
                ventO2BreathData.cieRR = MetricScaled[k++];
                ventO2BreathData.cieVtInsp = MetricScaled[k++];
                ventO2BreathData.cieVtExp = MetricScaled[k++];
                ventO2BreathData.cieMVinsp = MetricScaled[k++];
                ventO2BreathData.cieMVExp = MetricScaled[k++];
                ventO2BreathData.cieFiO2 = MetricScaled[k++];
                ventO2BreathData.ciePEEP = MetricScaled[k++];
                ventO2BreathData.ciePplat = MetricScaled[k++];
                ventO2BreathData.cieIE = MetricScaled[k++];
                ventO2BreathData.ciePpeak = MetricScaled[k++];
                ventO2BreathData.Pmean = MetricScaled[k++];
                ventO2BreathData.cieTi2Ttot = MetricScaled[k];

                if (ventO2BreathData.cieIE < (cieDataInvalid - 1) * 0.01) {
                    ventO2BreathData.calcExpTime = 60.0 / (ventO2BreathData.cieRR * (1.0 + ventO2BreathData.cieIE));
                    ventO2BreathData.calcInspTime = ventO2BreathData.cieIE * 60.0 / (ventO2BreathData.cieRR * (1.0 + ventO2BreathData.cieIE));
                } else {
                    ventO2BreathData.calcExpTime = (1 - ventO2BreathData.cieTi2Ttot) * 60 / ventO2BreathData.cieRR;
                    ventO2BreathData.calcInspTime = ventO2BreathData.cieTi2Ttot * 60 / ventO2BreathData.cieRR;
                }
                RunMode = End_Flag_Found;
            } else { // check when this is appliccable
                MetricNo++;
                MetricUnscaled[MetricNo] = 256 * NextSCI_chr;
                ByteCount = 1;
            }

            break;

            case Settings_Data:
            if (ByteCount == 2) {
                settingsUnscaled[settingsNo] = 256 * NextSCI_chr;
                --ByteCount;
            } else if (ByteCount == 1) {
                settingsUnscaled[settingsNo] += NextSCI_chr;
                --ByteCount;
                settingsScaled[settingsNo] = settingsUnscaled[settingsNo] * settingsScaleFactors[settingsNo] - settingsOffset[settingsNo];
            } else if (ByteCount == 0 && NextSCI_chr == EndFlag) {
                int k = 0;
                servoSettings.setRespRate = settingsScaled[k++];
                servoSettings.setMinuteVol = settingsScaled[k++];
                servoSettings.setPeep = settingsScaled[k++];
                servoSettings.setFiO2 = settingsScaled[k++];
                servoSettings.setInspPress = settingsScaled[k++];
                servoSettings.setVt = settingsScaled[k++];
                servoSettings.setVentMode = settingsScaled[k++];
                servoSettings.setPatRange = settingsScaled[k++];
                servoSettings.setComplianceCompensationOn = settingsScaled[k++];
                servoSettings.setIERatio = settingsScaled[k];
                RunMode = End_Flag_Found;
            } else {
                settingsNo++;
                settingsUnscaled[settingsNo] = 256 * NextSCI_chr;
                ByteCount = 1;
            }
            break;
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

char ServoCIEData::CRC_calc(String localstring) {
    char chk = 0;
    unsigned int len = localstring.length();
    for (int i = 0; i < len; i++) {
        chk = chk ^ localstring[i];
    }
    return chk;
}

void ServoCIEData::Send_SERVO_CMD(String InStr) {
    char CRC = CRC_calc(InStr);
    servoCom.print(InStr);
    if (CRC < 0x10) {
        servoCom.print("0");
    }
    servoCom.print(CRC, HEX);
    servoCom.write(EOT);
        hostCom.print(InStr);
        // if (CRC < 0x10) {
        //     hostCom.print("0");
        // }
        // hostCom.print(CRC, HEX);
}

void ServoCIEData::getCIEResponse() {
    delay(30);
    while (servoCom.available()) {
        inByte = servoCom.read();
        hostCom.print(' ');
        hostCom.print(inByte);
        hostCom.print(' ');
        hostCom.print(inByte, HEX);
        hostCom.print(' ');
        if (inByte == EOT) break;
    }
}

void ServoCIEData::CIE_setup() {
    servoCom.write(EOT);
     // hostCom.print("\nsending EOT ");
     getCIEResponse();
     // hostCom.print("\nsending ESC ");
     servoCom.write(ESC);
    getCIEResponse();
    // hostCom.print("\nsending RTIM ");
    Send_SERVO_CMD("RTIM");
    getCIEResponse();
    // hostCom.print("\nsending RCTY");
    Send_SERVO_CMD("RCTY");
    getCIEResponse();
    // hostCom.print("\nsending SDADB ");
    Send_SERVO_CMD("SDADB113114117100102101103104109108107122105106128");
    getCIEResponse();
    // hostCom.print("\nsending SDADS ");
    Send_SERVO_CMD("SDADS400405408414406420410409437419");
    getCIEResponse();
    // hostCom.print("\nsending SDADC ");
    Send_SERVO_CMD("SDADC000004001");
    getCIEResponse();
    // hostCom.print("\nsending RCCO102 ");
    Send_SERVO_CMD("RCCO102");
    getCIEResponse();
    // hostCom.print("\nsending RDAD ");
    Send_SERVO_CMD("RDAD");
    getCIEResponse();
    // hostCom.print("\nsending RADAB ");

    Send_SERVO_CMD("RADAB");
    delay(10);
    while (servoCom.available()) {
        inByte = servoCom.read();
        // hostCom.print("0");
        // hostCom.print(inByte, HEX);
        if (inByte == EOT) break;}
        // hostCom.print("\nSending RADAS ");
    Send_SERVO_CMD("RADAS");
    delay(10);
    while (servoCom.available()) {
        inByte = servoCom.read();
        // hostCom.print("0");
        // hostCom.print(inByte, HEX);
        if (inByte == EOT) break;
    }

    Send_SERVO_CMD("RADC");
    getCIEResponse();
}

