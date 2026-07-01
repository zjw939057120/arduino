#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>

#define MAX_BLE_ADDRESSES 10
#define FILTER_PARAM_MAX_LEN 32

void setupEntry();
void loopEntry();
int getEcnValue(wifi_auth_mode_t encryptionType);
void formatMacAddress(uint8_t* mac, char* output);
void hexToStr(uint8_t* data, int length, char* output);
bool parseWiFiCommand(char* cmd, char* ssid, char* pwd);
bool parseBLECommand(char* cmd, int* mode, int* duration, int* filter_type, char* filter_param);
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
void DebugSerialTask(void* pvParameters);
void MySerialTask(void* pvParameters);
void CommandTask(void* pvParameters);
void BLESensorTask(void* pvParameters);
bool parseBleListCommand(char* cmd, int* count, char macs[MAX_BLE_ADDRESSES][18]);
void saveBleListConfig(char macs[MAX_BLE_ADDRESSES][18]);
bool loadBleListConfig(char macs[MAX_BLE_ADDRESSES][18]);
void sendBleListReport(int count, char macs[MAX_BLE_ADDRESSES][18]);
int getATCWState();
void ATCWState();
void scanMode();
#endif // WIFI_SCAN_H
