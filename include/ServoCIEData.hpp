#ifndef SERVOCIE_DATA_HPP
#define SERVOCIE_DATA_HPP

#include <Arduino.h>
#include "main.hpp"
#include <FS.h>
#include <SPIFFS.h>
#include <string>
#include "DateTime.hpp"
#include "SDManager.hpp"  // Include the header for SDManager

extern DateTime dateTime;
extern WifiApServer WiFiserver; // only used to set text on home page

class SDManager; // forward declaration

struct Configs {
    String channel = "000";
    String label = "default";
    String unit = "N/A";
    float scaleFactor = 1.0;
    float offset = 0.0;
    uint16_t unscaled = 1;      // raw incoming value
    float scaled = 1;           // scaled/calibrated value
    bool active = true;     // default to true unless specified otherwise
};

class ServoCIEData {

public:
    ServoCIEData(SDManager* manager);
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
    void initializeConfigs(const char* metricPath, const char* settingPath, const char* curvePath = nullptr);
   
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
    static const int MaxCurves = 5;

    Configs metrics[MaxMetrics];
    int metricCount = 0;
    bool metricConfigLoaded = false;

    Configs settings[MaxSettings];
    int settingCount = 0;
    bool settingConfigLoaded = false;

    Configs curves[MaxCurves];
    int curveCount = 0;
    bool curveConfigLoaded = false;


    // bool begin();  // Initializes SPIFFS

    // config handling
    // bool loadConfigFromSD(const char* path);
    bool loadConfigFromSD(const char* path, Configs* configs, int& configCount, const int maxConfigs);
    // bool loadConfigFromSPIFFS(const char* path);
    bool loadConfigFromSPIFFS(const char* path, Configs* configs, int& configCount, const int maxConfigs);
    bool syncConfigSDToSPIFFS(const char* path);
    bool syncConfigSPIFFSToSD(const char* path);
    // void printAllConfig(const Configs* configsArray, int count);

    void printAllConfigs(const Configs* configs, const int numConfigs, const char* ConfigPaths);
    void printAllCurveConfigs();
    void printAllMetricsConfigs();
    void printAllSettingConfigs();

    // void scaleAll(int metricSize, int settingSize);
    void scaleCIEData(const float* unscaledArray, float* scaledArray, int count, const Configs* configsArray);
    String getUnitsAsString(const Configs* configsArray, int count) ;
    String getLabelsAsString(const Configs* configsArray, int count);
    String getScaledValuesAsString(const Configs* configsArray, int count, bool includeHeader = false);
    String getChannelsAsString(const Configs* configsArray, int count);

    String getServoID();    // returns servo type and serial num
    uint8_t getBreathPhase();   // return the present breath state, insp, exp or pause

private:

    SDManager* sdManager;  // Pointer to SDManager instan

    // SCI state management handlers:
    void handleAwaitingInfo(uint8_t NextSCI_chr);
    void handleBreathData(uint8_t NextSCI_chr);
    void handleSettingsData(uint8_t NextSCI_chr);
    void handleValueData(uint8_t NextSCI_chr);
    void handlePhaseData(uint8_t NextSCI_chr);
    void handleTrendData(uint8_t NextSCI_chr);
    void handleAlarmData(uint8_t NextSCI_chr);
    void handleErrorData(uint8_t NextSCI_chr);
    void handleRunMode(uint8_t NextSCI_chr);

    // communication timing stuff
    bool comOpen = false;
    unsigned long lastMessageTime = 0;
    unsigned long lastInitAttempt = 0;

    bool parseConfigLine(const String& line, Configs& c);

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

    // remove the following, use the Config Curves[n] instead
    uint16_t cieCurve1 = 0; // stores the flow signal received from CIE
    uint16_t cieCurve2 = 0; // stores the CO2 signal received from CIE
    uint16_t cieCurve3 = 0; // stores the airway pressure signal received from CIE
    uint16_t cieCurve4 = 0; // stores the Edi signal received from CIE
    uint16_t cieCurve5 = 0; // stores the Edi signal received from CIE

    // int cieFlow = 0;
    // int cieFCO2 = 0;
    // int ciePaw = 0;
    // int cieEdi = 0;

    uint8_t breathPhase = 0;

    unsigned int MetricNo = 0; // count which metric is currently being received
    unsigned int settingsNo = 0; // count which setting is currently being received
    unsigned int CurveCounter = 0; // Count which curve is presently being managed
    
    // #define NumberOfCurves 3 // Totalnumber of curves to receive from SCI/CIE
    // should use curveCount instead



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
