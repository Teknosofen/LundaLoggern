This project creates a small datalogger for a SERVO-u intensive care ventilator. 
It used a LilyGo T-dispaly S3 ESP-32 microcontroller with a dispaly, an SD-card reader as well as a RS232 transsciever.

The device reads configurations for which data to collect from a tile stored on the Sd-card or in SPIFFS and then continually logs data in a text file.
Data is time stamped with a time derived from teh conected device
the datalogger sets up a Wifi accesspoint and a simple homepage allowing download of data as well as review of settigns and configuration
