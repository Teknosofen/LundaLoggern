#pragma once
// -----------------------------------------------
//  HostCli — USB-CDC command/stream interface
//
//  Provides a simple line-oriented command parser on `hostCom` and gates
//  what data is mirrored from the CIE pipeline to the host PC.  Output is
//  tab-separated and human readable; every line carries a leading TAG so a
//  host-side tool can demultiplex Settings / Metrics / Alarms / Curves and
//  control / status traffic.
//
//  Tags used:
//    READY        — boot banner
//    OK / ERR     — command acknowledgement
//    INFO         — free-form informational text (also used for "Data from:")
//    DBG          — debug chatter (gated by DEBUG flag)
//    SETTING      — settings rows + their header line
//    METRIC       — metric rows + their header line
//    ALARM        — alarm rows + their header line
//    CURVE        — curve sample rows
//    HEADER       — explicit header on demand (HEADER C/M/S/A)
//    STATUS       — STATUS command output
//    FILE         — LIST output, one per file
//    LIST_END
//    DUMP_BEGIN / DUMP_END — bracket DUMP output (file content unmodified)
//    MARK         — user marker injected with MARK command
//
//  All commands are case-insensitive.  See `HELP` at runtime.
// -----------------------------------------------

#include <Arduino.h>

class SDManager;
class ServoCIEData;

class HostCli {
public:
    enum StreamBit : uint8_t {
        STREAM_SETTING = 1 << 0,
        STREAM_METRIC  = 1 << 1,
        STREAM_ALARM   = 1 << 2,
        STREAM_CURVE   = 1 << 3,
        STREAM_DEBUG   = 1 << 4,
    };

    HostCli();
    void attach(SDManager* sd, ServoCIEData* cie);
    void begin();      // call once after hostCom.begin()
    void update();     // call from loop()

    // --- emission API (called from data producers) -----------------------
    // `body` may contain multiple '\n'-terminated lines; each line will be
    // emitted with the TAG prefix.  Safe to call when stream is disabled
    // (becomes a no-op).
    void emitSetting(const String& body);
    void emitMetric (const String& body);
    void emitAlarm  (const String& body);

    // Called from the curve handler after a complete sample set has been
    // stored.  Honors the curve decimation factor and lazily emits a
    // header line on first sample after STREAM ON C.
    void emitCurveSample();

    // Always-on info / gated debug.  Both prepend their tag.
    void info(const String& line);
    void dbg (const String& line);

    bool streamEnabled(StreamBit s) const { return (streamMask & s) != 0; }
    bool debugEnabled() const { return streamEnabled(STREAM_DEBUG); }

private:
    SDManager*    sd  = nullptr;
    ServoCIEData* cie = nullptr;

    uint8_t  streamMask       = 0;   // all streams OFF by default
    uint16_t curveDecimation  = 1;   // 1 = every sample, N = every Nth
    uint16_t curveSampleCount = 0;
    bool     curveHeaderSent  = false;

    String   inBuf;                  // command-line accumulator

    // --- emission helpers ------------------------------------------------
    void emitTagged(const char* tag, const String& body);
    void emitLine  (const char* tag, const String& line);

    // --- command parsing -------------------------------------------------
    void handleLine(String line);
    void cmdStream (const String& args);
    void cmdDebug  (const String& args);
    void cmdStatus ();
    void cmdList   ();
    void cmdDump   (const String& filename);
    void cmdHeader (const String& which);
    void cmdMark   (const String& text);
    void cmdTime   (const String& arg);
    void cmdRate   (const String& args);
    void cmdFormat (const String& args);
    void cmdHelp   ();
    void cmdVersion();

    void respondOk (const String& msg);
    void respondErr(const String& msg);

    static uint8_t parseStreamLetters(const String& letters); // returns bitmask, 0 on error
    String streamMaskStr() const;
};

extern HostCli hostCli;
