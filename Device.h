#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>


// 是否为DEBUG环境
#define IS_DEBUG_ENV 0
// 固件版本号X.X.X
#define FW_VERSION 100
// BLE设备最大数量
#define MAX_BLE_ADDRESSES 10

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
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

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
  //版本号
  uint16_t screen_version;//屏幕版本号
  uint16_t system_version;//系统版本号
  uint16_t network_version;//网络版本号
  // WLAN配置
  char ssid[33];//当前连接的无线网络名称
  char pwd[65];//当前连接的无线网络密码
} SysConfig;// 系统配置结构体，用于存储当前连接的无线网络名称和密码

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
  char local_ip[16];//设备IP地址
  char gateway_ip[16];//网关IP地址
  char subnet_mask[16];//子网掩码
  char dns_ip[16];//DNS服务器IP地址

  //AP配置
  char ap_ssid[33];//AP无线网络名称
  char ap_pwd[65];//AP无线网络密码
  char mac[18];//MAC地址
} NetConfig;// 网络配置结构体，用于存储设备的IP地址、网关IP地址、子网掩码、DNS服务器IP地址、AP无线网络名称、AP无线网络密码、MAC地址

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
  bool ble_scaning;         // BLE扫描状态
  bool wifi_scaning;        // WiFi扫描状态
  uint8_t wifi_check_count; // WiFi检查次数，用于判断是否需要重新连接WiFi
} DeviceStatus;// 设备状态结构体，用于存储设备的BLE扫描状态、WiFi扫描状态和WiFi检查次数

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
  int baud;//波特率
  int dataBits;//数据位
  int stopBits;//停止位
  int parity;//校验位
  int addr;//地址位
} UartConfig;// 串口配置结构体，用于存储串口的波特率、数据位、停止位、校验位和地址位

// BLE传感器数据结构体
struct BLESensorData {
  // 温度传感器
  uint16_t temp;
  // 湿度传感器
  uint16_t hum;
  // 时间戳
  uint64_t timestamp;
};
// 传感器数据结构体
struct SensorData {
  uint16_t CO2; // 红外二氧化碳传感器CO2
  uint16_t CH2O; // 甲醛传感器CH2O
  uint16_t TVOC; // 空气质量传感器TVOC
  uint16_t PM25;  // 激光粉尘传感器PM2.5 GRIMM
  uint16_t PM100; // 激光粉尘传感器PM10 GRIMM
  uint16_t TEMP;  // 温度传感器温度值
  uint16_t RH;    // 湿度传感器湿度值
  uint16_t PM10;  // 激光粉尘传感器PM1.0 GRIMM
  uint8_t TYPE;  // 传感器类型
  uint16_t wifi_status; // wifi状态位
  uint16_t wifi_rssi;   // wifi信号强度
};

extern TaskHandles taskHandles;
extern SysConfig sysConfig;
extern NetConfig netConfig;
extern DeviceStatus deviceStatus;
extern UartConfig uartConfig;

extern char bleDevice[MAX_BLE_ADDRESSES][18];
extern int bleCount;
extern BLESensorData bleSensorData[MAX_BLE_ADDRESSES];
extern SensorData sensorData;

#endif // DEVICE_H
