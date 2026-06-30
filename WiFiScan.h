#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>

void setupEntry();
void loopEntry();
int getEcnValue(wifi_auth_mode_t encryptionType);
void formatMacAddress(uint8_t* mac, char* output);
void hexToStr(uint8_t* data, int length, char* output);
bool parseWiFiCommand(char* cmd, char* ssid, char* pwd);
bool parseBLECommand(char* cmd, int* mode, int* duration);
bool parseUartConfigCommand(char* cmd, int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr);
void saveWiFiConfig(char* ssid, char* pwd);
bool loadWiFiConfig(char* ssid, char* pwd);
void clearWiFiConfig();
void ScanWiFi();
void DoBLEScan(int duration);
bool autoConnect(char* ssid, char* pwd);
void DoWiFiConnect(char* ssid, char* pwd);
void processCommand(char* cmd);
#endif // WIFI_SCAN_H
