#include "Device.h"

// BLE设备MAC地址缓冲区
char bleDevice[MAX_BLE_ADDRESSES][18] = {{0}};
// BLE设备数量
int bleCount = 0;
// BLE传感器数据缓冲区
BLESensorData bleSensorData[MAX_BLE_ADDRESSES] = {0};
// 传感器数据缓冲区
SensorData sensorData = {0};
