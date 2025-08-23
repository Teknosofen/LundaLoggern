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
