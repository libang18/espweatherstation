ESPWEATHERSTATION (ESP32-S3 SUPERMINI & GC9A01)

A professional minimalist weather station built with the compact ESP32-S3 SuperMini and a GC9A01 circular display. This project features a smooth UI powered by LVGL and an easy code-free configuration via WiFiManager.

FEATURES
Modern Circular UI: Optimized for 240x240 px round displays
Smart Geocoding: Automatically finds coordinates for any city name using the Nominatim API
Live Weather: Real-time data temperature humidity and wind from the Open-Meteo API
Captive Portal: Easy WiFi and Location setup via smartphone no hardcoding credentials
Polished UX: Includes a Connecting status screen and non-blocking background updates

HARDWARE
Controller: ESP32-S3 SuperMini
Display: GC9A01 Circular LCD 240x240 SPI interface

WIRING (ESP32-S3 SUPERMINI)
GND to GND
VCC to 3.3V
SCL to 12
SDA to 11
RES to 8
DC to 9
CS to 10

LIBRARY SETUP (PRE-CONFIGURED)
1 Copy Libraries: Copy the provided lvgl and TFT_eSPI folders from the libraries folder of this repository into your local Arduino libraries folder
2 LVGL Config: Place the provided lv_conf.h file from the config folder directly into your root libraries folder. Note it must be at the same level as the library folders not inside them
3 Dependencies: Install WiFiManager and ArduinoJson via the Arduino Library Manager

INSTALLATION
1 Open ESPWeatherStation.ino in the Arduino IDE
2 Select ESP32S3 Dev Module as your board
3 Set USB CDC On Boot to Enabled
4 Upload the code to your ESP32-S3 SuperMini

FIRST TIME SETUP
1 Connect: Join the WiFi network named Meteo-Station-Setup on your phone
2 Configure WiFi: In the portal that appears click Configure WiFi select your network and enter your password
3 Location Settings: In the same form enter your city or address in the Location Settings field
4 Save: Click save. The device will restart connect to your WiFi and display live weather data
