// -----------------------------------------------
//  HostCli — implementation.  See HostCli.hpp for protocol details.
// -----------------------------------------------

#include "HostCli.hpp"
#include "main.hpp"           // hostCom, VERSION, LOGGER_SSID
#include "SDManager.hpp"
#include "ServoCIEData.hpp"
#include "DateTime.hpp"
#include <SD.h>
#include <WiFi.h>

HostCli hostCli;   // global instance, declared extern in header

HostCli::HostCli() {}

void HostCli::attach(SDManager* sdMgr, ServoCIEData* cieData) {
    sd  = sdMgr;
    cie = cieData;
}

void HostCli::begin() {
    inBuf.reserve(128);
    // Boot banner — single READY line so a PC tool can sync.
    hostCom.print("READY\t");
    hostCom.print(VERSION);
    hostCom.print('\t');
    hostCom.print(LOGGER_SSID);
    hostCom.println();
    hostCom.println("INFO\tType HELP for commands. All data streams are OFF at boot.");
}

// =============================================================
// Emission helpers
// =============================================================
void HostCli::emitLine(const char* tag, const String& line) {
    hostCom.print(tag);
    hostCom.print('\t');
    hostCom.print(line);
    // line is expected to NOT already include a trailing newline here;
    // caller (emitTagged) handles that.
}

// Prefix every '\n'-terminated line in `body` with "<tag>\t".
// Preserves trailing '\n's exactly; no extra newline is added.
void HostCli::emitTagged(const char* tag, const String& body) {
    const int len = body.length();
    if (len == 0) return;

    int start = 0;
    while (start < len) {
        int nl = body.indexOf('\n', start);
        if (nl < 0) {
            // tail without terminating newline
            hostCom.print(tag);
            hostCom.print('\t');
            hostCom.print(body.substring(start));
            hostCom.println();   // add a newline so each tagged line is complete
            return;
        }
        hostCom.print(tag);
        hostCom.print('\t');
        // include the '\n' from the source so blank lines stay blank
        hostCom.write(reinterpret_cast<const uint8_t*>(body.c_str() + start),
                      (size_t)(nl - start + 1));
        start = nl + 1;
    }
}

void HostCli::emitSetting(const String& body) {
    if (!streamEnabled(STREAM_SETTING)) return;
    emitTagged("SETTING", body);
}
void HostCli::emitMetric(const String& body) {
    if (!streamEnabled(STREAM_METRIC)) return;
    emitTagged("METRIC", body);
}
void HostCli::emitAlarm(const String& body) {
    if (!streamEnabled(STREAM_ALARM)) return;
    emitTagged("ALARM", body);
}

void HostCli::info(const String& line) {
    hostCom.print("INFO\t");
    hostCom.println(line);
}
void HostCli::dbg(const String& line) {
    if (!debugEnabled()) return;
    hostCom.print("DBG\t");
    hostCom.println(line);
}

void HostCli::emitCurveSample() {
    if (!streamEnabled(STREAM_CURVE)) return;
    if (!cie) return;

    // Lazy header emission on first sample after enabling the stream.
    if (!curveHeaderSent) {
        String labels = cie->getLabelsAsString(cie->curves, cie->curveCount);
        // labels already terminates with '\n'
        hostCom.print("CURVE\tTimestamp\t");
        hostCom.print(labels);
        curveHeaderSent = true;
        curveSampleCount = 0;
    }

    // Decimation
    if (curveDecimation > 1) {
        if ((curveSampleCount++ % curveDecimation) != 0) return;
    } else {
        curveSampleCount++;
    }

    hostCom.print("CURVE\t");
    hostCom.print(dateTime.getRTC().toString());
    for (int i = 0; i < cie->curveCount; ++i) {
        hostCom.print('\t');
        hostCom.print(cie->curves[i].scaled, 2);
    }
    hostCom.println();
}

// =============================================================
// Command parsing
// =============================================================
void HostCli::update() {
    while (hostCom.available()) {
        char c = (char)hostCom.read();
        if (c == '\r') continue;
        if (c == '\n') {
            String line = inBuf;
            inBuf = "";
            line.trim();
            if (line.length() > 0) handleLine(line);
            continue;
        }
        if (inBuf.length() < 200) {
            inBuf += c;
        } else {
            // overflow — drop and report once we see a newline
            inBuf = "<<OVERFLOW>>";
        }
    }
}

static String upperCopy(const String& s) {
    String r = s; r.toUpperCase(); return r;
}

void HostCli::handleLine(String line) {
    if (line == "<<OVERFLOW>>") { respondErr("input line too long"); return; }

    // split into <cmd> <rest>
    int sp = line.indexOf(' ');
    String cmd  = (sp < 0) ? line : line.substring(0, sp);
    String args = (sp < 0) ? String("") : line.substring(sp + 1);
    args.trim();
    cmd = upperCopy(cmd);

    if      (cmd == "HELP" || cmd == "?")  cmdHelp();
    else if (cmd == "STREAM")              cmdStream(args);
    else if (cmd == "DEBUG")               cmdDebug(args);
    else if (cmd == "STATUS")              cmdStatus();
    else if (cmd == "LIST")                cmdList();
    else if (cmd == "DUMP")                cmdDump(args);
    else if (cmd == "HEADER")              cmdHeader(args);
    else if (cmd == "MARK")                cmdMark(args);
    else if (cmd == "TIME")                cmdTime(args);
    else if (cmd == "RATE")                cmdRate(args);
    else if (cmd == "FORMAT")              cmdFormat(args);
    else if (cmd == "VERSION" || cmd == "VER") cmdVersion();
    else respondErr(String("unknown command: ") + cmd + " (try HELP)");
}

void HostCli::respondOk(const String& msg) {
    hostCom.print("OK\t");
    hostCom.println(msg);
}
void HostCli::respondErr(const String& msg) {
    hostCom.print("ERR\t");
    hostCom.println(msg);
}

// Parse a list of stream letters like "M,S,A" or "MSA" or "*".
// Returns the corresponding bitmask, or 0xFF if input is invalid.
uint8_t HostCli::parseStreamLetters(const String& letters) {
    if (letters.length() == 0) return 0xFF;
    if (letters == "*" || upperCopy(letters) == "ALL") {
        return STREAM_SETTING | STREAM_METRIC | STREAM_ALARM | STREAM_CURVE;
    }
    uint8_t mask = 0;
    String u = upperCopy(letters);
    for (size_t i = 0; i < u.length(); ++i) {
        char c = u[i];
        if (c == ',' || c == ' ' || c == '\t') continue;
        switch (c) {
            case 'S': mask |= STREAM_SETTING; break;
            case 'M': mask |= STREAM_METRIC;  break;
            case 'A': mask |= STREAM_ALARM;   break;
            case 'C': mask |= STREAM_CURVE;   break;
            default:  return 0xFF;
        }
    }
    return mask;
}

String HostCli::streamMaskStr() const {
    String r;
    r += (streamMask & STREAM_SETTING) ? 'S' : '-';
    r += (streamMask & STREAM_METRIC)  ? 'M' : '-';
    r += (streamMask & STREAM_ALARM)   ? 'A' : '-';
    r += (streamMask & STREAM_CURVE)   ? 'C' : '-';
    r += (streamMask & STREAM_DEBUG)   ? 'D' : '-';
    return r;
}

// STREAM ON M,S,A,C   /   STREAM OFF *   /   STREAM ?
void HostCli::cmdStream(const String& args) {
    if (args.length() == 0 || args == "?") {
        respondOk(String("STREAM ") + streamMaskStr());
        return;
    }
    int sp = args.indexOf(' ');
    String verb = upperCopy((sp < 0) ? args : args.substring(0, sp));
    String rest = (sp < 0) ? String("") : args.substring(sp + 1);
    rest.trim();

    if (verb == "ON" || verb == "OFF") {
        uint8_t bits = parseStreamLetters(rest);
        if (bits == 0xFF) { respondErr("STREAM: expected letters S/M/A/C or *"); return; }
        if (verb == "ON") {
            // arm header re-emission for newly enabled curve stream
            if ((bits & STREAM_CURVE) && !(streamMask & STREAM_CURVE)) {
                curveHeaderSent = false;
            }
            streamMask |= bits;
        } else {
            streamMask &= ~bits;
            if (!(streamMask & STREAM_CURVE)) curveHeaderSent = false;
        }
        respondOk(String("STREAM ") + streamMaskStr());
        return;
    }
    respondErr("STREAM: expected ON | OFF | ?");
}

// DEBUG ON|OFF|?
void HostCli::cmdDebug(const String& args) {
    String v = upperCopy(args);
    if (v == "" || v == "?") {
        respondOk(String("DEBUG ") + (debugEnabled() ? "ON" : "OFF"));
        return;
    }
    if (v == "ON")  { streamMask |=  STREAM_DEBUG; respondOk("DEBUG ON");  return; }
    if (v == "OFF") { streamMask &= ~STREAM_DEBUG; respondOk("DEBUG OFF"); return; }
    respondErr("DEBUG: expected ON | OFF | ?");
}

// STATUS — emit a block of STATUS\t<key>=<value> lines, terminated by OK.
void HostCli::cmdStatus() {
    hostCom.print("STATUS\tfw=");         hostCom.println(VERSION);
    hostCom.print("STATUS\tservoID=");    hostCom.println(cie ? cie->getServoID() : String("?"));
    hostCom.print("STATUS\tcomOpen=");    hostCom.println(cie && cie->isComOpen() ? "1" : "0");
    hostCom.print("STATUS\tsdPresent=");  hostCom.println(sd  && sd->isCardPresent() ? "1" : "0");
    hostCom.print("STATUS\trtc=");        hostCom.println(dateTime.getRTC().toString());
    hostCom.print("STATUS\tpsramFree=");  hostCom.println(ESP.getFreePsram());
    hostCom.print("STATUS\theapFree=");   hostCom.println(ESP.getFreeHeap());
    hostCom.print("STATUS\twifiClients="); hostCom.println(WiFi.softAPgetStationNum());
    hostCom.print("STATUS\tstream=");     hostCom.println(streamMaskStr());
    hostCom.print("STATUS\tcurveRate=1/"); hostCom.println(curveDecimation);
    respondOk("STATUS");
}

// LIST — enumerate root of SD card.
void HostCli::cmdList() {
    if (!sd || !sd->isCardPresent()) { respondErr("LIST: SD not present"); return; }
    File root = SD.open("/");
    if (!root || !root.isDirectory()) { respondErr("LIST: cannot open /"); return; }
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            hostCom.print("FILE\t");
            hostCom.print(entry.name());
            hostCom.print('\t');
            hostCom.println(entry.size());
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
    hostCom.println("LIST_END");
    respondOk("LIST");
}

// DUMP <filename> — bracket file contents between DUMP_BEGIN/DUMP_END.
// File contents are emitted verbatim (no per-line tagging) so a PC tool can
// just stream the same parser it uses on the SD file directly.
void HostCli::cmdDump(const String& filename) {
    if (filename.length() == 0) { respondErr("DUMP: missing filename"); return; }
    if (!sd || !sd->isCardPresent()) { respondErr("DUMP: SD not present"); return; }

    String path = filename.startsWith("/") ? filename : ("/" + filename);
    File f = SD.open(path, FILE_READ);
    if (!f) { respondErr(String("DUMP: cannot open ") + path); return; }

    hostCom.print("DUMP_BEGIN\t"); hostCom.print(path);
    hostCom.print('\t');           hostCom.println(f.size());

    // stream the file in modest chunks
    uint8_t buf[128];
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        if (n == 0) break;
        hostCom.write(buf, n);
    }
    f.close();

    // ensure DUMP_END starts on its own line
    hostCom.println();
    hostCom.print("DUMP_END\t");   hostCom.println(path);
    respondOk(String("DUMP ") + path);
}

// HEADER S|M|A|C — emit current label header without waiting for a new file.
void HostCli::cmdHeader(const String& which) {
    if (!cie) { respondErr("HEADER: CIE not ready"); return; }
    String w = upperCopy(which);
    if (w.length() != 1) { respondErr("HEADER: expected S | M | A | C"); return; }

    String labels;
    const char* tag = nullptr;
    switch (w[0]) {
        case 'S': labels = cie->getLabelsAsString(cie->settings, cie->settingCount); tag = "SETTING"; break;
        case 'M': labels = cie->getLabelsAsString(cie->metrics,  cie->metricCount);  tag = "METRIC";  break;
        case 'C': labels = cie->getLabelsAsString(cie->curves,   cie->curveCount);   tag = "CURVE";   break;
        case 'A': labels = cie->getAlarmLabelsAsString();                            tag = "ALARM";   break;
        default:  respondErr("HEADER: expected S | M | A | C"); return;
    }
    hostCom.print(tag);
    hostCom.print("\tTimestamp\t");
    hostCom.print(labels);                       // already ends with '\n'
    respondOk(String("HEADER ") + w);
}

// MARK <text> — inject a timestamped marker on the USB stream.
void HostCli::cmdMark(const String& text) {
    hostCom.print("MARK\t");
    hostCom.print(dateTime.getRTC().toString());
    hostCom.print('\t');
    hostCom.println(text);
    respondOk("MARK");
}

// TIME [iso] — query the RTC.  Setting the RTC from the host is not
// implemented here on purpose; the firmware syncs it from the ventilator.
void HostCli::cmdTime(const String& arg) {
    if (arg.length() > 0) {
        respondErr("TIME: setting RTC from host is not supported (RTC is synced from ventilator)");
        return;
    }
    respondOk(String("TIME ") + dateTime.getRTC().toString());
}

// RATE C <n> — emit every Nth curve sample.  Decimation only applies to
// curves for now (metrics arrive once per breath, settings on change).
void HostCli::cmdRate(const String& args) {
    if (args.length() == 0) {
        respondOk(String("RATE C 1/") + curveDecimation);
        return;
    }
    int sp = args.indexOf(' ');
    if (sp < 0) { respondErr("RATE: expected '<stream> <n>'"); return; }
    String which = upperCopy(args.substring(0, sp));
    String num   = args.substring(sp + 1); num.trim();
    long n = num.toInt();
    if (n < 1 || n > 10000) { respondErr("RATE: n must be 1..10000"); return; }
    if (which != "C") { respondErr("RATE: only curve stream (C) supports decimation"); return; }
    curveDecimation = (uint16_t)n;
    curveSampleCount = 0;
    respondOk(String("RATE C 1/") + curveDecimation);
}

// FORMAT — only TSV is supported in this build, but the command is
// accepted so a PC tool can probe / set it without erroring.
void HostCli::cmdFormat(const String& args) {
    if (args.length() == 0 || upperCopy(args) == "?") {
        respondOk("FORMAT TSV");
        return;
    }
    if (upperCopy(args) == "TSV") { respondOk("FORMAT TSV"); return; }
    respondErr("FORMAT: only TSV is supported in this build");
}

void HostCli::cmdVersion() {
    respondOk(String("VERSION ") + VERSION);
}

void HostCli::cmdHelp() {
    const char* lines[] = {
        "INFO\tCommands (case-insensitive):",
        "INFO\t  HELP | ?                 - show this list",
        "INFO\t  VERSION                  - firmware version",
        "INFO\t  STATUS                   - device status block",
        "INFO\t  STREAM ON  S|M|A|C|*     - enable data stream(s)",
        "INFO\t  STREAM OFF S|M|A|C|*     - disable data stream(s)",
        "INFO\t  STREAM ?                 - show current mask (SMACD)",
        "INFO\t  DEBUG ON | OFF | ?       - toggle DBG\\t... lines",
        "INFO\t  HEADER S | M | A | C     - re-emit header for that stream",
        "INFO\t  RATE C <n>               - emit every Nth curve sample",
        "INFO\t  FORMAT [TSV|?]           - only TSV supported",
        "INFO\t  LIST                     - list files on SD root",
        "INFO\t  DUMP <filename>          - replay an SD log over USB",
        "INFO\t  MARK <text>              - inject a timestamped marker",
        "INFO\t  TIME                     - show RTC time"
    };
    for (auto* l : lines) hostCom.println(l);
    respondOk("HELP");
}
