#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

#if ARDUINO_AirM2M_CORE_ESP32C3
HardwareSerial MySerial(0);
#else
HardwareSerial MySerial(1);
#endif

#endif // CONFIG_H