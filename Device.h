#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>

// 网络版本号
#define NETWORK_VERSION 0
// BLE设备最大数量
#define MAX_BLE_ADDRESSES 10

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
  // 系统配置
  uint16_t screen_version;//屏幕版本号
  uint16_t system_version;//系统版本号
  uint16_t network_version;//网络版本号
  // WiFi配置
  char ssid[33];//当前连接的无线网络名称
  char pwd[65];//当前连接的无线网络密码
} SystemConfig;// 系统配置结构体，用于存储当前连接的无线网络名称和密码

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

typedef struct {
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
  // 红外二氧化碳传感器CM1106S
  uint16_t CO2; // CO2
  // 甲醛传感器SC11-CH2O
  uint16_t CH2O; // CH2O
  // 空气质量传感器MS-VOC-V4
  uint16_t TVOC; // TVOC
  // 激光粉尘传感器PM2012SE
  uint16_t PM25;  // PM2.5 GRIMM
  uint16_t PM100; // PM10 GRIMM
  // 温度传感器
  uint16_t TEMP;
  // 湿度传感器
  uint16_t RH;
  // 激光粉尘传感器PM2012SE
  uint16_t PM10;  // PM1.0 GRIMM
  // 传感器类型
  uint8_t TYPE;
};

extern TaskHandles taskHandles;
extern SystemConfig systemConfig;
extern DeviceConfig deviceConfig;
extern DeviceStatus deviceStatus;
extern UartConfig uartConfig;

extern char bleDevice[MAX_BLE_ADDRESSES][18];
extern int bleCount;
extern BLESensorData bleSensorData[MAX_BLE_ADDRESSES];
extern SensorData sensorData;

#endif // DEVICE_H
