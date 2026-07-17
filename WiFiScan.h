#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>
#include "Device.h"

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
} TaskHandles;// 任务句柄结构体，用于存储所有任务的句柄

typedef struct {
  char ssid[33];//当前连接的无线网络名称
  char pwd[65];//当前连接的无线网络密码
} WiFiConfig;// WiFi配置结构体，用于存储当前连接的无线网络名称和密码

typedef struct {
  char local_ip[16];//设备IP地址
  char gateway_ip[16];//网关IP地址
  char subnet_mask[16];//子网掩码
  char dns_ip[16];//DNS服务器IP地址

  //AP配置
  char ap_ssid[33];//AP无线网络名称
  char ap_pwd[65];//AP无线网络密码
  char mac[18];//MAC地址
} DeviceConfig;// 设备配置结构体，用于存储设备的IP地址、网关IP地址、子网掩码、DNS服务器IP地址、AP无线网络名称、AP无线网络密码、MAC地址

typedef struct {
  bool ble_scaning;         // BLE扫描状态
  bool wifi_scaning;        // WiFi扫描状态
  uint8_t wifi_check_count; // WiFi检查次数，用于判断是否需要重新连接WiFi
} DeviceStatus;// 设备状态结构体，用于存储设备的BLE扫描状态、WiFi扫描状态和WiFi检查次数

extern TaskHandles taskHandles;
extern WiFiConfig wifiConfig;
extern DeviceConfig deviceConfig;
extern DeviceStatus deviceStatus;

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
void saveWiFiConfig(char* ssid, char* pwd);
bool loadWiFiConfig(char* ssid, char* pwd);
bool parseDeviceConfigCommand(char* cmd, char* local_ip, char* gateway_ip, char* subnet_mask, char* dns_ip);
void doSaveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void saveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void sendDeviceConfigReport(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
void loadDeviceConfig();
void configStation();
void wifiConnect();
void restore();
void ScanWiFi();
void ScanWiFiHandler(char* content, int size);
void DoBLEScan(int duration);
void DoWiFiConnect(const char* ssid, const char* pwd);
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
