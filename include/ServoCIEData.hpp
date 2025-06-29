#ifndef SERVOCIE_DATA_HPP
#define SERVOCIE_DATA_HPP

#include <Arduino.h>
#include "main.hpp"

class ServoCIEData {
public:
    ServoCIEData();
    void begin();
    void parseCIEData(char NextSCI_chr);
    void getCIEResponse();
    void CIE_setup();

private:
    enum RunModeType {
        Awaiting_Info,
        End_Flag_Found,
        Breath_Data,
        Value_Data,
        Phase_Data,
        Trend_Data,
        Alarm_Data,
        Error_Data,
        Settings_Data,
        Run_Mode,
        StandBy
        // Add other modes as needed
    };

    RunModeType RunMode;
    // int RunMode;
    int ByteCount;
    int phase;
    int Error_info;
    int inByte;     // collects the latest char from the ventilator

    int cieFlow = 0; // stores the flow signal received from CIE
    int cieFCO2 = 0;
    int ciePaw = 0;
    int cieEdi = 0;

    // curves / waveforms
    int CurveCounter = 0; // Count which curve is presently being managed
    #define NumberOfCurves 3 // Totalnumber of curves to receive from SCI/CIE

    // struct to store flow, CO2, and airway pressure curve data from the ventilator
    // Message ID 20, data rate about 100Hz
    // Åke L 2021-04-20
    //
    #define ventO2CurveDataMsgID 20
    typedef struct  {
                                                                // Airway flow is channel 0,  is channel +2500E-004, +4000E+000, 02, CU
    float cieFlow = 0.0;                                      // 100 Hz scaled flow curve sample [ml/s], 4 bytes
                                                                // CO2 is channel 4, +1000E-004, +0000E+000, 07, CU
    float cieCO2 = 0.0;                                       // 100 Hz scaled CO2 curve sample [%], 4 bytes
                                                                // Paw is channel 1, +1000E-004, +2000E-001, 04, CU
    float ciePaw = 0.0;                                       // 100 Hz scaled airway pressure curve sample [cmH2O], 4 bytes
    uint32_t ciePhase = 0;                                    // phase info, updated when changed but available at each 100 Hz package, 4 bytes
    } VentO2CurveDataStruct;
    VentO2CurveDataStruct ventO2CurveData;
    VentO2CurveDataStruct* ventO2CurveDataPtr = &ventO2CurveData;

    // Metrics
    #define NumMetrics 15 // number of metrics collected from SCI
    int16_t MetricUnscaled[NumMetrics]; // raw data from CIE, CIE obviously sends negative values where they should be posistiie
    float MetricScaled[NumMetrics]; // scaled metrics
    float MetricOffset[NumMetrics+1] =       {0.0,     0.0,      0.0,      0.0,     0.0,      0.0,     0.0,      0.0,     0.0,     200.0,  200.0,   0.0,  200.0,   200.0,   0.0};
    float MetricScaleFactors[NumMetrics+1] = {0.1,     0.1,      1.0,      0.1,     0.2,      0.2,     0.01,     0.01,    0.1,     0.1,    0.1,     0.01, 0.1,     0.1,     0.0001};
    //             channel no:                #113      #114     #117      #100     #101      #102     #103      #104     #109    #108     #107     #122  #105     ¤106     #128
    String MetricLabels[NumMetrics+1] =      {"VtCO2", "FetCO2", "MVCO2",  "RR",    "VtInsp", "VtExp", "MVinsp", "MVExp", "FiO2", "PEEP",  "EIP",   "IE", "PPeak", "Pmean", "Ti/Ttot"};
    String MetricUnits[NumMetrics+1] =       {"ml",     "%",     "ml/min", "1/min", "mL",     "mL",    "L/min",  "L/min", "%",    "cmH2O", "cmH2O", "-",  "cmH2O", "chH2O", "-"};
    int MetricsHaveArrived = false; // Used to signal that all metrics have arrived safely and should be output to host
    
    unsigned int MetricNo; // count which metric is currently being received

    // Struct to store breath 2 breath data from ventilator
    // Message ID 21, data rate <= Hz
    // Åke L 2022-04-29
    // Update the #define NumMetrics in the servoCIE.h if you change number of metrics to receive from CIE/SCI
    #define ventBreathO2DataID 21
    typedef struct  {
    float cieVtCO2 = 0.0;                                     // scaled tidal production CO2 [mL], 4 bytes, chan #113, [ml]
    float cieFetCO2 = 0.0;                                    // scaled end-tidal CO2 [%], 4 bytes, chan #114, [%]
    float cieMVCO2 = 0.0;                                     // scaled minute production CO2 [ml/min], 4 bytes, chan #117, [ml/min]
    float cieRR = 0.0;                                        // scaled resp rate [1/min], chan #100, +1000E-004, +0000E+000, 06, BR
    float cieVtInsp = 0.0;                                    // scaled insp tidal volume [ml]], chan #101, +2000E-004, +0000E+000, 01, BR
    float cieVtExp = 0.0;                                     // scaled exp tidal volume [ml]], chan #102 +2000E-004, +0000E+000, 01, BR
    float cieMVinsp = 0.0;                                    // scaled insp minute volume rate [L/min], chan #103 +1000E-005, +0000E+000, 08, BR
    float cieMVExp = 0.0;                                     // scaled exp minute volume rate [L/min], chan #104 +1000E-005, +0000E+000, 08, BR
    float cieFiO2 = 0.0;                                      // scaled FiO2 [vol%], chan #109, +1000E-004, +0000E+000, 07, BR
    float ciePEEP = 0.0;                                      // scaled PEEP, chan # 108 +1000E-004, +2000E-001, 04, BR
    float ciePplat = 0.0;                                     // scaled plateau press, #107
    float ciePpeak = 0.0;                                     // scaled peak press chan #105 +1000E-004, +2000E-001, 04, BR
    // float cieEIP = 0.0;                                    // scaled drive press, #148
    float cieIE = 0.0 ;                                       // IE ratio chan #122 +1000E-005, +0000E+000, 20, BR
    float cieTi2Ttot = 0.0 ;                                  // IE ratio chan #128 +1000E-007, +0000E+000, 20, BR
    float calcExpTime = 1;                                    // exp time in [s]
    float calcInspTime = 1;                                   // insp time in [s]
    float calcInspFlow = 0.0;                                 // insp flow from RR, Vt etc
    float calcExpFlow = 0.0;                                  // exp flow from RR, Vt etc
    float calcInspFlowFiltered = 0.0;                         // filtered insp flow from RR, Vt etc
    float calcExpFlowFiltered = 0.0;                          // filtered exp flow from RR, Vt etc

    float deltaP = 0.0;                                       // breath pressure swing, set_P_insp - PEEP or Pplat - PEEP.
    float Pmean = 0.0 ;                                       // mean airway pressure, channel #106, +1000E-004, +2000E-001, 04, BR

    // additional data 2021-11-17
    float calcB2Btime = 0.0;                                  // Stores the time interval between two received metrics sets from the ventilator 
    float calcB2Btimefiltered = 0.0;                          // Stores a filtered version, filter algo TBD
    float calcInspFlowMedianFiltered = 0.0;                   // filtered insp flow from RR, Vt etc, will use a 8 sample median fulter until furtner notice
    float calcExpFlowMedianFiltered = 0.0;                    // filtered exp flow from RR, Vt etc, will use a 8 sample median fulter until furtner notice
    
    } ventO2BreathDataStruct;
    ventO2BreathDataStruct ventO2BreathData;
    ventO2BreathDataStruct* ventO2BreathDataPtr = &ventO2BreathData;


    // settings
    #define numSettings 10 // number of settings collected from SCI
    uint16_t settingsUnscaled[numSettings]; // raw data from CIE
    float settingsScaled[numSettings]; // scaled settings
    float settingsOffset[numSettings] =       {0.0,     0.0,     0.0,     0.0,    0.0,     0.0,  0.0,    0.0,      0.0,        0.0};
    float settingsScaleFactors[numSettings] = {0.1,     0.01,    0.1,     0.1,    0.1,     0.2,  1,      1,        1,          0.01};
    String settingsLabels[numSettings] =      {"RR",    "MV",    "PEEP",  "FiO2", "Pinsp", "Vt", "mode", "PatCat", "ComplComp", "IE"};
    String settingsUnits[numSettings] =       {"1/min", "L/min", "cmH2O", "%",    "cmH2O", "mL", "N/A",  "N/A",    "N/A",       "N/A"};
    int settingsHaveArrived = false; // Used to signal that all metrics have arrived safely and should be output to host

    unsigned int settingsNo; // count which setting is currently being received

        // servo u settingslist
    // Message ID 22, data rate <= Hz, sent when any of the settings has been changed
    // Åke L 2021-04-20
    // Update the #define numSettings in the servoCIE.h if you change this
    #define ventilatorSettingListDataID 22
    typedef struct  {
    float setRespRate = 0;                                    // chan 400,  +1000E-004, +0000E+000, 06, SD
    float setMinuteVol = 0.0;                                 // chan 405, +1000E-005, +0000E+000, 08, SD
    float setPeep = 0.0;                                      // chan 408, +1000E-004, +0000E+000, 04, SD
    float setFiO2 = 0.0;                                      // chan 414, mode in list
    float setInspPress = 0.0;                                 // chan 406,  +1000E-004, +0000E+000, 04, SD
    float setVt = 0.0;                                        // chan 420 +2000E-004, +0000E+000, 01, SD
    uint32_t setVentMode = 0.0;                               // chan 410, mode in list
    uint32_t setPatRange = 0.0;                               // chan 409, setting in list
    uint32_t setComplianceCompensationOn = 0;                 // chan #437 gives info om the received airway flow signal
    float setIERatio = 0.0;                                   // chan 419 I:E Ratio+1000E-005, +0000E+000, 20, SD
    } servoSettingList;
    servoSettingList servoSettings;
    servoSettingList* myServoSettingListPtr = &servoSettings;

    // Add other private members as needed
    void Send_SERVO_CMD(String InStr);
    void ScaleMetrics();
    char CRC_calc(String localstring);

    // CIE constants
    // -------------
    #define EOT 0x04  // end of text
    #define ESC 0x1B  // escape char
    #define CR 0x0D   // CR char
    #define LF 0x0A   // LF char
    #define ValueFlag 0x80  // 
    #define PhaseFlag 0x81  //
    #define ErrorFlag 0xE0  //
    #define EndFlag   0x7F  //

    #define cieDataInvalid 0x7EFF  // 0x7EFF,= 32511 flags that cie data is not available or invalid
    #define cieEIPInvalid 3000.0 // limit just below a scaled version of the 0x7EFF
};

#endif // PARSECIE_DATA_HPP
