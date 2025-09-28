This project creates a small datalogger for a SERVO-u intensive care ventilator. 
It used a LilyGo T-dispaly S3 ESP-32 microcontroller with a dispaly, an SD-card reader as well as a RS232 transsciever.

The device reads configurations for which data to collect from a file stored on the Sd-card or in SPIFFS and then continually logs data in a text file.
Data is time stamped with a time derived from the conected device.
The datalogger sets up a Wifi accesspoint and a simple homepage allowing download and deletion of data files as well as review of settings and configuration
During operation the display presents info about what device is connected as well as its serial number.
