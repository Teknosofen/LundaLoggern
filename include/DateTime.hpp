#pragma once
#include <Arduino.h>  // for String on ESP32/Arduino

class DateTime {
public:
    // Constructors
    DateTime();  // invalid by default
    DateTime(int year, int month, int day,
             int hour, int minute, int second,
             bool valid = true);

    // Accessors
    int  year()   const { return _year; }
    int  month()  const { return _month; }
    int  day()    const { return _day; }
    int  hour()   const { return _hour; }
    int  minute() const { return _minute; }
    int  second() const { return _second; }
    bool isValid() const { return _valid; }

    // Mutators
    void setYear(int v)   { _year = v; }
    void setMonth(int v)  { _month = v; }
    void setDay(int v)    { _day = v; }
    void setHour(int v)   { _hour = v; }
    void setMinute(int v) { _minute = v; }
    void setSecond(int v) { _second = v; }
    void setValid(bool v) { _valid = v; }

    // Helpers
    // Human-readable: "YYYY-MM-DD HH:MM:SS"
    String toString() const;

    // Raw compact form: "YYYYMMDDhhmmss"
    String rawString() const;

    // RTC Integration
    void setRTC() const;
    void readRTC();

private:
    int  _year{0};
    int  _month{0};
    int  _day{0};
    int  _hour{0};
    int  _minute{0};
    int  _second{0};
    bool _valid{false};
};
