#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>

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

#define MAX_BLE_ADDRESSES 10

extern char bleDevice[MAX_BLE_ADDRESSES][18];
extern int bleCount;
extern BLESensorData bleSensorData[MAX_BLE_ADDRESSES];
extern SensorData sensorData;

#endif // DEVICE_H
