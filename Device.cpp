#include "Device.h"

// 任务句柄
TaskHandles taskHandles = {NULL,NULL, NULL,NULL, NULL, NULL};
// 系统配置
SystemConfig systemConfig = {.screen_version = NETWORK_VERSION, .system_version = NETWORK_VERSION, .network_version = NETWORK_VERSION};
// 设备配置
DeviceConfig deviceConfig;
// 设备状态
DeviceStatus deviceStatus = {false, false, 0};
// 串口配置
UartConfig uartConfig = {9600, 8, 1, 0, 1};

// BLE设备MAC地址缓冲区
char bleDevice[MAX_BLE_ADDRESSES][18] = {{0}};
// BLE设备数量
int bleCount = 0;
// BLE传感器数据缓冲区
BLESensorData bleSensorData[MAX_BLE_ADDRESSES] = {0};
// 传感器数据缓冲区
SensorData sensorData = {0};
