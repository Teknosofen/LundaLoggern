#ifndef SERVOCIE_DATA_HPP
#define SERVOCIE_DATA_HPP

#include <Arduino.h>
#include "main.hpp"
#include <FS.h>
#include <SPIFFS.h>
#include <string>
#include "DateTime.hpp"


// when in stdvy, the CIE does not send any data, so we need to poll it
// there seems to be an err message sent that could deserve a dedicated state?

// #define hostCom Serial          // potential duplicate

// new 2025-08-04
// struct Metric {
//     String channel;
//     String label;
//     String unit;
//     float scaleFactor;
//     float offset;
// };

// struct Setting {
//     String channel;
//     String label;
//     String unit;
//     float scaleFactor;
//     float offset;
// };

struct Configs {
    String channel;
    String label;
    String unit;
    float scaleFactor;
    float offset;
    float unscaled;   // raw incoming value
    float scaled;     // scaled/calibrated value
};


class ServoCIEData {

public:
    ServoCIEData();
    bool begin();
    void parseCIEData(char NextSCI_chr);
    String getCIEResponse();

    // --- Parsers ---
    DateTime parseRTIMResponse(const char* response, size_t len);
    void parseRCTYResponse(const char* response, size_t len);
    void parseRSENResponse(const char* response, size_t len);

    bool CIE_comCheck();
    bool CIE_setup();
    
    // --- Command sending ---
    void Send_SERVO_CMD(const char* InStr);
    void Send_SERVO_CMD_ASCII(const char* InStr);

        // --- CRC ---
    uint8_t CRC_calc(const char* localstring);
    void CRC_calcASCII(const char* localstring, char* outHex); // New CRC function: returns 2 ASCII hex characters via outHex
   
    String concatConfigChannels(const Configs configs[], int numConfigs);
    void initializeConfigs(const char* metricPath, const char* settingPath);
   
    // Communication status
    void setComOpen(bool state);
    bool isComOpen() const;

    // Init attempt timestamp
    void setLastInitAttempt(unsigned long timestamp);
    unsigned long getLastInitAttempt() const;

    // Last message timestamp
    void setLastMessageTime(unsigned long timestamp);
    unsigned long getLastMessageTime() const;
    
    // Configs
    static const int MaxMetrics = 20;
    static const int MaxSettings = 20;

    Configs metrics[MaxMetrics];
    int metricCount = 0;
    bool metricConfigLoaded = false;

    Configs settings[MaxSettings];
    int settingCount = 0;
    bool settingConfigLoaded = false;

    // bool begin();  // Initializes SPIFFS

    // Metric config
    bool loadMetricFromSD(const char* path);
    bool loadMetricFromSPIFFS(const char* path);
    bool syncMetricSDToSPIFFS(const char* path);
    bool syncMetricSPIFFSToSD(const char* path);
    void printAllMetrics();

    // Setting config
    bool loadSettingFromSD(const char* path);
    bool loadSettingFromSPIFFS(const char* path);
    bool syncSettingSDToSPIFFS(const char* path);
    bool syncSettingSPIFFSToSD(const char* path);
    void printAllSettings();

    // void scaleAll(int metricSize, int settingSize);
    void scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray);
    String getUnitsAsString(const Configs* configsArray, int count) ;
    String getLabelsAsString(const Configs* configsArray, int count);
    String getScaledValuesAsString(const float* scaledArray, int count);
    String getChannelsAsString(const Configs* configsArray, int count);

    String getServoID();    // returns servo type and serial num

private:
    // communication timing stuff
    bool comOpen = false;
    unsigned long lastMessageTime = 0;
    unsigned long lastInitAttempt = 0;

    bool parseMetricLine(const String& line, Configs& m);
    bool parseSettingLine(const String& line, Configs& s);

    // Generic ASCII response parser with optional status flag
    String parseASCIIResponse(const char* response, size_t len, bool* statusError = nullptr);
    
    // Parsed device info
    String servoID = "";  // from RCTY
    String servoSN = "";  // from RSN

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

    static constexpr char EOT = 0x04;
    static constexpr char ESC = 0x1B;

    // 🔒 Static command strings
    // Mutable buffers for command strings
    char CMD_RTIM[8];
    char CMD_RCTY[8];
    char CMD_RSEN[8];
    char CMD_SDADB[64];
    char PAYLOAD_SDADB[64];
    char PAYLOAD_SDADS[64];
    char CMD_SDADS[64];
    char CMD_SDADC[32];
    char PAYLOAD_SDADC[32];
    char CMD_RCCO[16];
    char CMD_RDAD[8];
    char CMD_RADAB[8];
    char CMD_RADAS[8];
    char CMD_RADC[8];

    char tempCommand[128];


    RunModeType RunMode;
    // int RunMode;
    int ByteCount;
    int phase;
    int Error_info;
    char inByte;     // collects the latest char from the ventilator

    int cieFlow = 0; // stores the flow signal received from CIE
    int cieFCO2 = 0;
    int ciePaw = 0;
    int cieEdi = 0;

    unsigned int MetricNo; // count which metric is currently being received
    unsigned int settingsNo; // count which setting is currently being received

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

    // CIE constants
    // -------------
    #define EOT 0x04  // end of text
    #define ESC 0x1B  // escape char
    #define CR  0x0D   // CR char
    #define LF  0x0A   // LF char
    #define ValueFlag 0x80  // 
    #define PhaseFlag 0x81  //
    #define ErrorFlag 0xE0  //
    #define EndFlag   0x7F  //

    #define StdbyErr    0x11 // error code sent when in stdby
    #define BuffFullErr 0x13 // error code sent when buffer is full
    #define ESCErr      0x14 // error code sent when transmission stopped by ESC
    

    #define cieDataInvalid 0x7EFF  // 0x7EFF,= 32511 flags that cie data is not available or invalid
    #define cieEIPInvalid 3000.0 // limit just below a scaled version of the 0x7EFF
};

#endif // PARSECIE_DATA_HPP
