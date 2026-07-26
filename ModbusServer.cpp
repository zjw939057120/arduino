#include "ModbusServer.h"
#include <ESPmDNS.h>
#include "WiFiScan.h"
#include "Device.h"

NetworkServer modbusServer;

// 寄存器地址映射: 按MQTT发布顺序
// 0:CO2, 1:CH2O, 2:TVOC, 3:PM25, 4:PM100, 5:TEMP, 6:RH, 7:PM10, 8:TYPE
// 9~28: bleSensorData[0~9].temp, bleSensorData[0~9].hum
uint16_t getModbusRegister(uint16_t addr) {
  switch (addr) {
    case 0: return sensorData.CO2;
    case 1: return sensorData.CH2O;
    case 2: return sensorData.TVOC;
    case 3: return sensorData.PM25;
    case 4: return sensorData.PM100;
    case 5: return sensorData.TEMP;
    case 6: return sensorData.RH;
    case 7: return sensorData.PM10;
    case 8: return sensorData.TYPE;
    default:
      if (addr >= 9 && addr <= 28) {
        uint16_t idx = addr - 9;
        if (idx % 2 == 0) {
          return bleSensorData[idx / 2].temp;
        } else {
          return bleSensorData[idx / 2].hum;
        }
      }
      return 0;
  }
}

void ModbusServerStart() {
  delay(5000); // 等待5秒，确保WiFi连接稳定

  // 启动Modbus TCP服务器
  modbusServer.begin(MODBUS_PORT);
  Serial.println("MODBUS server started");
}

void ModbusServerHandler() {
  NetworkClient client = modbusServer.accept();

  if (client) {
    Serial.println("Modbus Client Connected.");

    while (client.connected()) {
      if (client.available()) {
        // 读取 MBAP 头部 (7字节)
        uint8_t mbap[7];
        if (client.read(mbap, 7) != 7) {
          break;
        }

        uint16_t transactionId = (mbap[0] << 8) | mbap[1];
        uint16_t protocolId = (mbap[2] << 8) | mbap[3];
        uint16_t length = (mbap[4] << 8) | mbap[5];
        uint8_t unitId = mbap[6];

        // 验证协议ID
        if (protocolId != 0) {
          break;
        }

        // 读取 PDU
        uint8_t pdu[256];
        int pduLen = length - 1; // 减去 Unit ID
        if (pduLen <= 0 || pduLen > 256) {
          break;
        }

        if (client.read(pdu, pduLen) != pduLen) {
          break;
        }

        uint8_t functionCode = pdu[0];
        uint8_t response[256];
        int responseLen = 0;

        // 处理功能码
        switch (functionCode) {
          case 0x03: // Read Holding Registers
          case 0x04: // Read Input Registers
          {
            if (pduLen < 5) break;
            uint16_t startAddr = (pdu[1] << 8) | pdu[2];// 起始地址
            uint16_t quantity = (pdu[3] << 8) | pdu[4];// 读取寄存器数量

            // 构建响应
            response[0] = functionCode;
            response[1] = quantity * 2; // 字节数
            for (int i = 0; i < quantity; i++) {
              uint16_t value = getModbusRegister(startAddr + i);
              response[2 + i * 2] = (value >> 8) & 0xFF;
              response[3 + i * 2] = value & 0xFF;
            }
            responseLen = 2 + quantity * 2;
            break;
          }

          case 0x06: // Write Single Register
          {
            if (pduLen < 5) break;
            // 回写确认
            response[0] = functionCode;
            response[1] = pdu[1]; // 寄存器地址高字节
            response[2] = pdu[2]; // 寄存器地址低字节
            response[3] = pdu[3]; // 值高字节
            response[4] = pdu[4]; // 值低字节
            responseLen = 5;
            break;
          }

          default:
            // 不支持的功能码，返回异常响应
            response[0] = functionCode | 0x80;
            response[1] = 0x01; // 非法功能码
            responseLen = 2;
            break;
        }

        // 构建 MBAP 响应头部
        uint8_t responseMbap[7];
        responseMbap[0] = (transactionId >> 8) & 0xFF;
        responseMbap[1] = transactionId & 0xFF;
        responseMbap[2] = 0; // 协议ID高字节
        responseMbap[3] = 0; // 协议ID低字节
        uint16_t respLength = responseLen + 1; // PDU长度 + Unit ID
        responseMbap[4] = (respLength >> 8) & 0xFF;
        responseMbap[5] = respLength & 0xFF;
        responseMbap[6] = unitId;

        // 发送响应
        client.write(responseMbap, 7);
        client.write(response, responseLen);
      }
    }

    client.stop();
    Serial.println("Modbus Client Disconnected.");
  }
}
