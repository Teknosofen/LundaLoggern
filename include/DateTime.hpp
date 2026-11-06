#pragma once
// =============================================================================
//  DateTime.hpp — calendar date/time value and access to the system clock
// -----------------------------------------------------------------------------
//  The logger has no battery-backed RTC. Its "RTC" is the ESP32 system clock,
//  which starts at 1970 after every reset and is set from the ventilator's
//  clock (SCI command RTIM) during CIE init. All log timestamps and file names
//  derive from it.
//
//  The ventilator's local time is stored as-is. No time zone is configured on
//  the ESP32 (so it is treated as UTC), and localtime() returns the same
//  wall-clock values that were set.
// =============================================================================
#include <Arduino.h>  // for String on ESP32/Arduino


class DateTime {
public:
    /// Invalid date/time (all fields 0).
    DateTime();
    DateTime(int year, int month, int day,
             int hour, int minute, int second,
             bool valid = true);

    // Accessors. month 1-12, day 1-31, hour 0-23.
    int  year()    const { return _year; }
    int  month()   const { return _month; }
    int  day()     const { return _day; }
    int  hour()    const { return _hour; }
    int  minute()  const { return _minute; }
    int  second()  const { return _second; }
    bool isValid() const { return _valid; }

    // Mutators (no range checking)
    void setYear(int v)   { _year =   v; }
    void setMonth(int v)  { _month =  v; }
    void setDay(int v)    { _day =    v; }
    void setHour(int v)   { _hour =   v; }
    void setMinute(int v) { _minute = v; }
    void setSecond(int v) { _second = v; }
    void setValid(bool v) { _valid =  v; }

    /// "YYYY-MM-DD HH:MM:SS", or "Invalid DateTime". Used in every log row.
    String toString() const;

    /// "YYYYMMDDhhmmss" (the RTIM format), or "" if invalid.
    String rawString() const;

    // --- System clock ("RTC") ---
    /// Set the ESP32 system clock to this value (validity is not checked).
    void setRTC() const;
    /// Set the system clock to dt, but only if dt is valid.
    static void setRTC(const DateTime& dt);
    /// Load this object from the system clock; invalid if the clock was never set.
    void readRTC();
    /// Current system clock as a DateTime. See readRTC() for the blocking caveat.
    static DateTime getRTC();

private:
    int  _year{0};
    int  _month{0};
    int  _day{0};
    int  _hour{0};
    int  _minute{0};
    int  _second{0};
    bool _valid{false};
};
