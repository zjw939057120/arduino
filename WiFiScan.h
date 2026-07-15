#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>
#include "Sensor.h"

typedef struct {
    TaskHandle_t debugSerialTaskHandle;
    TaskHandle_t mySerialTaskHandle;
    TaskHandle_t bleTaskHandle;
    TaskHandle_t mqttTaskHandle;
    TaskHandle_t modbusTaskHandle;
    TaskHandle_t httpServerTaskHandle;
    TaskHandle_t commandTaskHandle;
    TaskHandle_t networkTaskHandle;
    TaskHandle_t miscTaskHandle;

} TaskHandles;

typedef struct {
  char ssid[33];//当前连接的无线网络名称
  char pwd[65];//当前连接的无线网络密码
} WiFiConfig;

typedef struct {
  char local_ip[16];//设备IP地址
  char gateway_ip[16];//网关IP地址
  char subnet_mask[16];//子网掩码
  char dns_ip[16];//DNS服务器IP地址

  //AP配置
  char ap_ssid[33];//AP无线网络名称
  char ap_pwd[65];//AP无线网络密码
  char mac[18];//MAC地址
} DeviceConfig;

extern TaskHandles taskHandles;
extern WiFiConfig wifiConfig;
extern DeviceConfig deviceConfig;

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
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr);
void saveWiFiConfig(char* ssid, char* pwd);
bool loadWiFiConfig(char* ssid, char* pwd);
void loadDeviceConfig();
void restore();
void ScanWiFi();
void DoBLEScan(int duration);
void DoWiFiConnect(char* ssid, char* pwd);
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
void ATCWState();
int findBleDevice(const char* addr);
int sendBleSensorData();
bool containsNonASCII(const char* ssid);
void getMacAddress(char *macStr);
void getHostname(char *hostnameStr);

#endif // WIFI_SCAN_H
