#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>
#include "Device.h"

// 串口缓冲区大小
#define SERIAL_BUFFER_SIZE 255
// 系统配置命名空间
#define NVS_SYS_NAMESPACE "sys_config"
// 网络配置命名空间
#define NVS_NET_NAMESPACE "net_config"
// UART配置命名空间
#define NVS_UART_NAMESPACE "uart_config"
// BLE配置命名空间
#define NVS_BLE_NAMESPACE "ble_config"
// MQTT配置命名空间
#define NVS_MQTT_NAMESPACE "mqtt_config"
// 命令队列大小
#define COMMAND_QUEUE_SIZE 8


// AT指令
#define AT_CMD_AT "AT"
// 重启指令
#define AT_CMD_RESTART "AT+RST"
// 列出当前可用的AP
#define AT_CMD_CWLWAP "AT+CWLAP"
// 断开与AP的连接
#define AT_CMD_CWQAP "AT+CWQAP"
// 连接WiFi指令
#define AT_CMD_CWJAP "AT+CWJAP="
// 获取连接WiFi信息指令
#define AT_CMD_CWJAP_GET "AT+CWJAP?"
// BLE扫描指令
#define AT_CMD_BLE_SCAN "AT+BLESCAN="
// 设置BLE列表指令
#define AT_CMD_BLE_LST "AT+BLE_LST="
// 获取BLE列表指令
#define AT_CMD_BLE_LST_GET "AT+BLE_LST?"
// 设置UART定义指令
#define AT_CMD_UART_DEF "AT+UART_DEF="
// 获取UART定义指令
#define AT_CMD_UART_DEF_GET "AT+UART_DEF?"
// 获取WiFi状态指令
#define AT_CMD_CWSTATE_GET "AT+CWSTATE?"
// 设置传感器数据指令
#define AT_CMD_SENSOR "AT+SENSOR="
// 设置MQTT配置指令
#define AT_CMD_MQTT_DEF "AT+MQTT_DEF="
// 获取MQTT配置指令
#define AT_CMD_MQTT_DEF_GET "AT+MQTT_DEF?"
// 设置网络配置指令
#define AT_CMD_DEVICE_DEF "AT+DEVICE_DEF="
// 获取网络配置指令
#define AT_CMD_DEVICE_DEF_GET "AT+DEVICE_DEF?"
// 设置版本指令
#define AT_CMD_VERSION "AT+VERSION="
// 获取版本指令
#define AT_CMD_VERSION_GET "AT+VERSION?"

typedef enum {
  CW_STATE_IDLE = 0,//空闲状态
  CW_STATE_CONNECTED = 1,//已连接状态
  CW_STATE_CONNECTED_WITH_IP = 2,//已连接且有IP地址状态
  CW_STATE_SCAN_COMPLETED = 3,//扫描完成状态
  CW_STATE_DISCONNECTED = 4,//已断开连接状态
} cw_state_t;

#if IS_DEBUG_ENV
// 调试串口
#define MySerial Serial
#else
// 串口1
#define MySerial Serial1
#endif

// 初始化任务
void setupEntry();
// 循环任务
void loopEntry();
// 获取ECN值
int getEcnValue(wifi_auth_mode_t encryptionType);
// 格式化MAC地址
void formatMacAddress(uint8_t* mac, char* output);
// 将十六进制字符串转换为字节数组
void hexToStr(uint8_t* data, int length, char* output);
// 解析WiFi命令
bool parseWiFiCommand(char* cmd, char* ssid, char* pwd);
// 解析BLE命令
bool parseBLECommand(char* cmd, int* mode, int* duration, int* filter_type, char* filter_param);
// 解析串口配置命令
bool parseUartConfigCommand(char* cmd, int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
// 解析传感器命令
bool parseSensorCommand(char* cmd, SensorData* data);
// 解析MQTT命令
bool parseMQTTCommand(char* cmd, char* ip, int* port, char* username, char* password, char* prefix);
// 保存MQTT配置
void saveMQTTConfig(char* ip, int port, char* username, char* password, char* prefix);
// 加载MQTT配置
bool loadMQTTConfig(char* ip, int* port, char* username, char* password, char* prefix);
// 发送MQTT配置报告
void sendMQTTConfigReport(char* ip, int port, char* username, char* password, char* prefix);
// 发送WiFi事件报告
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
// 加载串口配置
bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr);
// 保存串口配置
void doSaveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
// 保存串口配置
void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr);
// 发送串口配置报告
void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr);
// 保存系统配置
void saveSysConfig();
// 加载系统配置
bool loadSysConfig();
// 解析网络配置命令
bool parseNetConfigCommand(char* cmd, char* local_ip, char* gateway_ip, char* subnet_mask, char* dns_ip);
// 保存网络配置
void doSaveNetConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
// 保存网络配置
void saveNetConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
// 发送网络配置报告
void sendNetConfigReport(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip);
// 加载网络配置
void loadNetConfig();
// 配置WiFi站
void configStation();
// 连接WiFi
void wifiConnect();
// 恢复默认配置
void restore();
// 发送CWJAP报告
void sendCWJAPReport();
// 发送扫描WiFi报告
void SendScanWiFiReport();
// 扫描WiFi处理函数
void ScanWiFiHandler(char* content, int size);
// 执行BLE扫描
void DoBLEScan(int duration);
// 连接WiFi
void wifiConnect();
// 禁用WiFi
void disableWiFi();
// 连接WiFi
void DoWiFiConnect(const char* ssid, const char* pwd);
// 解析版本命令
bool parseVersionCommand(char* cmd);
// 发送版本报告
void sendVersionReport();
// 处理命令
void processCommand(char* cmd);
// 调试串口任务
void DebugSerialTask(void* pvParameters);
// 自定义串口任务
void MySerialTask(void* pvParameters);
// BLE传感器任务
void BLESensorTask(void* pvParameters);
// Modbus服务器任务
void ModbusServerTask(void* pvParameters);
// HTTP服务器任务
void HttpServerTask(void* pvParameters);
// MQTT订阅任务
void MQTTSubClientTask(void* pvParameters);
// 命令任务
void CommandTask(void* pvParameters);
// 网络任务
void NetworkTask(void* pvParameters);
// 其他任务
void MiscTask(void* pvParameters);
// 解析BLE列表命令
bool parseBleListCommand(char* cmd, int* count);
// 保存BLE列表配置
void saveBleListConfig();
// 发送BLE列表报告
void sendBleListReport(int count);
// 获取AT CW状态
uint8_t getATCWState();
// 发送AT CW状态报告
void sendCWStateReport();
// 查找BLE设备
int findBleDevice(const char* addr);
// 发送BLE传感器报告
int sendBleSensorReport();
// 检查字符串是否包含非ASCII字符
bool containsNonASCII(const char* ssid);
// 获取MAC地址
void getMacAddress(char *macStr);
// 获取主机名
void getHostname(char *hostnameStr);

#endif // WIFI_SCAN_H
