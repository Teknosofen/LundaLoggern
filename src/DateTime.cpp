// =============================================================================
//  DateTime.cpp — date/time value and ESP32 system clock access.
//  See DateTime.hpp for how the clock is set.
// =============================================================================

#include "DateTime.hpp"
#include <cstdio> // snprintf

DateTime::DateTime()
    : _year(0), _month(0), _day(0),
      _hour(0), _minute(0), _second(0),
      _valid(false) {}

DateTime::DateTime(int year, int month, int day,
                   int hour, int minute, int second,
                   bool valid)
    : _year(year), _month(month), _day(day),
      _hour(hour), _minute(minute), _second(second),
      _valid(valid) {}

String DateTime::toString() const {
    if (!_valid) return "Invalid DateTime";
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             _year, _month, _day, _hour, _minute, _second);
    return String(buf);
}

String DateTime::rawString() const {
    if (!_valid) return "";
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d",
             _year, _month, _day, _hour, _minute, _second);
    return String(buf);
}


// --- System clock -----------------------------------------------------------

// struct tm counts years from 1900 and months from 0.
void DateTime::setRTC() const {
    struct tm timeinfo;
    timeinfo.tm_year = _year - 1900;
    timeinfo.tm_mon  = _month - 1;
    timeinfo.tm_mday = _day;
    timeinfo.tm_hour = _hour;
    timeinfo.tm_min  = _minute;
    timeinfo.tm_sec  = _second;
    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, nullptr);
}

// getLocalTime() (Arduino-ESP32) treats any year up to 2016 as "clock not
// set" and keeps retrying for up to its default timeout of 5 s before it
// gives up. So until the ventilator has provided the time, every call to
// readRTC()/getRTC() blocks for about 5 s and returns an invalid DateTime.
void DateTime::readRTC() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        _valid = false;
        return;
    }
    _year   = timeinfo.tm_year + 1900;
    _month  = timeinfo.tm_mon + 1;
    _day    = timeinfo.tm_mday;
    _hour   = timeinfo.tm_hour;
    _minute = timeinfo.tm_min;
    _second = timeinfo.tm_sec;
    _valid  = true;
}

void DateTime::setRTC(const DateTime& dt) {
    if (dt.isValid()) {
        dt.setRTC();
    }
}

DateTime DateTime::getRTC() {
    DateTime dt;
    dt.readRTC();
    return dt;
}
