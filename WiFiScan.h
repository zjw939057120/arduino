#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>
#include "Device.h"

void setupEntry();
void loopEntry();
int getEcnValue(wifi_auth_mode_t encryptionType);
void formatMacAddress(uint8_t* mac, char* output);
void hexToStr(uint8_t* data, int length, char* output);
bool parseWiFiCommand(char* cmd, char* ssid, char* pwd);
bool parseBLECommand(char* cmd, int* mode, int* duration, int* filter_type, char* filter_param);
bool parseUartConfigCommand(char* cmd, int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
bool parseSensorCommand(char* cmd, SensorData* data);
bool parseMQTTCommand(char* cmd, char* ip, int* port, char* username, char* password);
void saveMQTTConfig(char* ip, int port, char* username, char* password);
bool loadMQTTConfig(char* ip, int* port, char* username, char* password);
void sendMQTTConfigReport(char* ip, int port, char* username, char* password);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
void doSaveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr);
void saveSystemConfig(char* ssid, char* pwd);
bool loadSystemConfig(char* ssid, char* pwd);
bool parseDeviceConfigCommand(char* cmd, char* local_ip, char* gateway_ip, char* subnet_mask, char* dns_ip);
void doSaveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void saveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void sendDeviceConfigReport(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void loadDeviceConfig();
void configStation();
void wifiConnect();
void restore();
void sendCWJAPReport();
void SendScanWiFiReport();
void ScanWiFiHandler(char* content, int size);
void DoBLEScan(int duration);
void DoWiFiConnect(const char* ssid, const char* pwd);
bool parseVersionCommand(char* cmd);
void sendVersionReport();
void processCommand(char* cmd);
void DebugSerialTask(void* pvParameters);
void MySerialTask(void* pvParameters);
void BLESensorTask(void* pvParameters);
void ModbusServerTask(void* pvParameters);
void HttpServerTask(void* pvParameters);
void MQTTSubClientTask(void* pvParameters);
void CommandTask(void* pvParameters);
void NetworkTask(void* pvParameters);
void MiscTask(void* pvParameters);
bool parseBleListCommand(char* cmd, int* count);
void saveBleListConfig();
void sendBleListReport(int count);
uint8_t getATCWState();
void sendCWStateReport();
int findBleDevice(const char* addr);
int sendBleSensorReport();
bool containsNonASCII(const char* ssid);
void getMacAddress(char *macStr);
void getHostname(char *hostnameStr);

#endif // WIFI_SCAN_H
