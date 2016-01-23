# ESP8266-Temperature-Monitor
Wifi enabled temperature monitor - with web api and uploading to Thingsspeak (https://thingspeak.com/)

Also implements simple timed jobs, logging, json web api.

Uses:
- ESP8266 v1
- DHT22

Uses arduino libraries:
  - ESP8266WiFi.h - Wifi connection handling
  - ESP8266WebServer.h - Provides a lightweight web server
  - TimeLib.h - Provides time utilities
  - DHT.h - Provides temperature control for DHT versions

