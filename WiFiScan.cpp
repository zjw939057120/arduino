#include "Config.h"
#include "WiFiScan.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ModbusTCP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#define SERIAL_BUFFER_SIZE 128
#define NVS_NAMESPACE "wifi_config"
#define NVS_UART_NAMESPACE "uart_config"
#define NVS_BLE_NAMESPACE "ble_config"
#define COMMAND_QUEUE_SIZE 8

typedef struct {
  char cmd[SERIAL_BUFFER_SIZE];
} CommandMessage;

char serialBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;
Preferences preferences;
BLEScan* pBLEScan;
static QueueHandle_t commandQueue = NULL;
// Pending WiFi SSID and password
static char pendingSSID[33] = "";
static char pendingPWD[65] = "";

#define MAX_BLE_ADDRESSES 10
// BLE device MAC addresses
char bleMacs[MAX_BLE_ADDRESSES][18] = {{0}};
// BLE device count
int bleCount = 0;
// BLE sensor data buffer
struct BLESensorData {
  uint16_t temp;
  uint16_t hum;
};
BLESensorData bleSensorData[MAX_BLE_ADDRESSES] = {0};

#define FILTER_PARAM_MAX_LEN 20
int filter_type = 3;// 0: no filter, 1: filter by MAC, 2: filter by name, 3: filter by service UUID
char filter_param[FILTER_PARAM_MAX_LEN] = "0000ffe0";// filter parameter

class MyBLEScanCallback : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice device) {
    String addr = device.getAddress().toString();
    addr.toUpperCase();
    int rssi = device.getRSSI();

    uint8_t* advData = device.getPayload();
    size_t advLen = device.getPayloadLength();

    String name = device.haveName() ? device.getName() : "";
    String serviceData = device.haveServiceData() ? device.getServiceData(0) : "";
    String serviceUUID = device.haveServiceUUID() ? device.getServiceUUID(0).toString() : "";
    uint8_t addrType = device.getAddressType();

  bool passFilter = true; // 默认通过过滤
  switch (filter_type)
  {
  case 0: // NONE
    break;
  case 1: // MAC
    passFilter = strcmp(filter_param, addr.c_str()) <= 0;
    break;
  case 2: // NAME
    passFilter = strcmp(filter_param, name.c_str()) <= 0;
    break;
  case 3: // UUID
    passFilter = strcmp(filter_param, serviceUUID.c_str()) <= 0;
    break;
  case 4: // RSSI
    passFilter = device.getRSSI() >= atoi(filter_param);
    break;
  default:
    break;
  }
  if (passFilter) {
    char advDataStr[255] = "";
    for (size_t i = 0; i < advLen && i < 255; i++) {
      sprintf(advDataStr + i * 2, "%02X", advData[i]);
    }
    MySerial.printf("+BLESCAN:\"%s\",%d,%s,%s,%s,%d\r\n", addr.c_str(), rssi, advDataStr, serviceData.c_str(), serviceUUID.c_str(), addrType);
  }
  }
};

class MyBLESensorCallback : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice device) {
    String addr = device.getAddress().toString();
    addr.toUpperCase();

    uint8_t* advData = device.getPayload();
    size_t advLen = device.getPayloadLength();
    String serviceUUID = device.haveServiceUUID() ? device.getServiceUUID(0).toString() : "";

    bool passFilter = strcmp(filter_param, serviceUUID.c_str()) <= 0;
    // 过滤不匹配的设备
    if (!passFilter)
      return;
      //查找设备
    int index = findBleDevice(addr.c_str());
    if (index == -1)
      return;
    int8_t rssi = WiFi.RSSI();
    // 温度数据
    bleSensorData[index].temp = ((uint8_t)advData[advLen - 3] << 8) | (uint8_t)advData[advLen - 4];
    // 湿度数据
    bleSensorData[index].hum = ((uint8_t)advData[advLen - 1] << 8) | (uint8_t)advData[advLen - 2];
    // char advDataStr[255] = "";
    // for (size_t i = 0; i < advLen && i < 255; i++) {
    //   sprintf(advDataStr + i * 2, "%02X", advData[i]);
    // }
    // Serial.printf("+SENSOR:%d,\"%s\",%s,%d\r\n", index, addr.c_str(), advDataStr, rssi);
  }
};

// BLE扫描锁
bool ble_scan_lock = false;

// BLE扫描回调
MyBLEScanCallback bleScanCallback;
// BLE传感器回调
MyBLESensorCallback bleSensorCallback;
int getEcnValue(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:            return 0;
    case WIFI_AUTH_WEP:             return 1;
    case WIFI_AUTH_WPA_PSK:         return 2;
    case WIFI_AUTH_WPA2_PSK:        return 3;
    case WIFI_AUTH_WPA_WPA2_PSK:    return 4;
    case WIFI_AUTH_WPA2_ENTERPRISE: return 5;
    case WIFI_AUTH_WPA3_PSK:        return 6;
    case WIFI_AUTH_WPA2_WPA3_PSK:   return 7;
    case WIFI_AUTH_WAPI_PSK:        return 8;
    default:                        return 0;
  }
}

void formatMacAddress(uint8_t* mac, char* output) {
  sprintf(output, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void hexToStr(uint8_t* data, int length, char* output) {
  int index = 0;
  for (int i = 0; i < length; i++) {
    index += sprintf(output + index, "%02X", data[i]);
  }
  output[index] = '\0';
}

bool parseWiFiCommand(char* cmd, char* ssid, char* pwd) {
  char* paramStart = cmd + 9;
  char* comma = strchr(paramStart, ',');
  
  if (comma == NULL) {
    return false;
  }
  
  *comma = '\0';
  char* start = paramStart;
  char* end = paramStart + strlen(paramStart);
  
  if (*start == '"') {
    start++;
  }
  if (*(end - 1) == '"') {
    *(end - 1) = '\0';
  }
  
  strncpy(ssid, start, 32);
  ssid[31] = '\0';
  
  char* pwdStart = comma + 1;
  start = pwdStart;
  end = pwdStart + strlen(pwdStart);
  
  if (*start == '"') {
    start++;
  }
  if (*(end - 1) == '"') {
    *(end - 1) = '\0';
  }
  
  strncpy(pwd, start, 64);
  pwd[63] = '\0';
  
  return true;
}

bool parseBLECommand(char* cmd, int* mode, int* duration, int* filter_type, char* filter_param) {
    // 1. 定位参数起始位置 ("AT+BLESCAN=" 长度为 11)
    char* paramStart = cmd + 11;
    
    // 2. 解析必选参数 <enable>
    char* comma = strchr(paramStart, ',');
    if (comma == NULL) {
        // 如果没有逗号，说明只有 enable 参数，根据规范需返回 false
        return false; 
    }

    *comma = '\0'; // 截断字符串
    *mode = atoi(paramStart);
    
    // 3. 解析必选参数 <duration>
    char* token = comma + 1;
    comma = strchr(token, ',');
    
    if (comma == NULL) {
        // 只有 duration，没有 filter 参数
        *duration = atoi(token);
        *filter_type = 0; // 默认无过滤
        *filter_param = '\0';
    } else {
        // 存在 filter 参数
        *comma = '\0'; // 截断字符串
        *duration = atoi(token);
        
        // 4. 解析可选参数 <filter_type>
        token = comma + 1;
        comma = strchr(token, ',');
        
        if (comma == NULL) {
            // 只有 filter_type，没有 filter_param (格式不规范，返回 false)
            return false; 
        }
        
        *comma = '\0'; // 截断字符串
        *filter_type = atoi(token);
        
        // 5. 解析可选参数 <filter_param>
        token = comma + 1;
        if (*token == '\0') {
            // filter_param 为空，格式不规范
            return false; 
        }
        
        // 将 filter_param 拷贝到目标缓冲区，防止缓冲区溢出
        strncpy(filter_param, token, FILTER_PARAM_MAX_LEN - 1);
        filter_param[FILTER_PARAM_MAX_LEN - 1] = '\0';
        // 去除首尾双引号
        size_t len = strlen(filter_param);
        if (len >= 2 && filter_param[0] == '"' && filter_param[len - 1] == '"') {
          // 将结束符前移，去掉尾部双引号
          filter_param[len - 1] = '\0';
          // 将指针整体后移一位，去掉首部双引号
          // 注意：如果 filter_param 是动态分配的内存，直接修改指针会导致内存泄漏
          // 如果 filter_param 是固定数组，不能直接修改指针，需要用 memmove
          memmove(filter_param, filter_param + 1, len - 1);
        }
        
    }

    // 6. 参数合法性校验
    // mode 必须为 1 (开始扫描)
    if (*mode != 1) return false;
    
    // duration 必须在 1~60 秒之间 (0 表示持续扫描，视具体需求而定，这里按你的原逻辑保留)
    if (*duration <= 0 || *duration > 60) return false;
    
    // 如果设置了过滤类型，校验其范围 (1-4)
    if (*filter_type != 0 && (*filter_type < 1 || *filter_type > 4)) {
        return false; 
    }

    return true;
}

bool parseUartConfigCommand(char* cmd, int* baud, int* dataBits, int* stopBits, int* parity, int* addr) {
  char* paramStart = cmd + 12;
  char* next = strchr(paramStart, ',');
  if (next == NULL) {
    return false;
  }
  *next = '\0';
  *baud = atoi(paramStart);

  char* token = next + 1;
  next = strchr(token, ',');
  if (next == NULL) {
    return false;
  }
  *next = '\0';
  *dataBits = atoi(token);

  token = next + 1;
  next = strchr(token, ',');
  if (next == NULL) {
    return false;
  }
  *next = '\0';
  *stopBits = atoi(token);

  token = next + 1;
  if (token == NULL || *token == '\0') {
    return false;
  }

  next = strchr(token, ',');
  if (next == NULL) {
    *parity = atoi(token);
    *addr = 0;
  } else {
    *next = '\0';
    *parity = atoi(token);
    token = next + 1;
    if (token == NULL || *token == '\0') {
      return false;
    }
    *addr = atoi(token);
  }

  // Validation: ESP32-C3 ranges and requested numeric encoding
  if (*baud < 80 || *baud > 5000000) return false;
  if (*dataBits < 5 || *dataBits > 8) return false;
  if (*stopBits < 1 || *stopBits > 3) return false; // 1=1,2=1.5,3=2
  if (*parity < 0 || *parity > 2) return false; // 0=None,1=Odd,2=Even
  if (*addr < 0 || *addr > 255) return false;

  return true;
}

bool parseBleListCommand(char* cmd, int* count) {
  char* paramStart = cmd + 11; // skip "AT+BLE_LST="
  
  // Parse first parameter: count
  char* comma = strchr(paramStart, ',');
  if (comma == NULL) {
    return false;
  }
  *comma = '\0';
  *count = atoi(paramStart);
  
  if (*count <= 0 || *count > MAX_BLE_ADDRESSES) {
    return false;
  }
  
  // Parse MAC addresses
  char* token = comma + 1;
  int macIdx = 0;
  
  while (macIdx < *count && token != NULL) {
    comma = strchr(token, ',');
    if (comma != NULL) {
      *comma = '\0';
    }

    char* start = token;
    char* end = token + strlen(token);

    if (*start == '"') {
      start++;
    }
    if (end > start && *(end - 1) == '"') {
      *(end - 1) = '\0';
    }

    if (*start == '\0') {
      return false;
    }

    strncpy(bleMacs[macIdx], start, 17);
    bleMacs[macIdx][17] = '\0';
    macIdx++;

    if (comma == NULL) {
      break;
    }
    token = comma + 1;
  }

  if (macIdx != *count) {
    return false;
  }

  return true;
}

void saveBleListConfig() {
  preferences.begin(NVS_BLE_NAMESPACE, false);
  for (int i = 0; i < MAX_BLE_ADDRESSES; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mac_%d", i);
    preferences.putString(key, bleMacs[i]);
  }
  preferences.end();
}

bool loadBleListConfig() {
  preferences.begin(NVS_BLE_NAMESPACE, true);
  bool hasAny = false;
  for (int i = 0; i < MAX_BLE_ADDRESSES; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mac_%d", i);
    String value = preferences.getString(key, "");
    if (value.length() > 0) {
      strncpy(bleMacs[i], value.c_str(), 17);
      bleMacs[i][17] = '\0';
      hasAny = true;
    } else {
      bleMacs[i][0] = '\0';
    }
  }
  preferences.end();
  return hasAny;
}

void sendBleListReport(int count) {
  MySerial.printf("+BLE_LST:%d", count);
  for (int i = 0; i < count; ++i) {
    if (bleMacs[i][0] != '\0') {
      MySerial.printf(",\"%s\"", bleMacs[i]);
    } else {
      MySerial.print(",\"\"");
    }
  }
  MySerial.println();
}

uint8_t disconnected_num = 0; // 断线次数
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      // 重置断线次数
      disconnected_num = 0;
      WiFi.setAutoReconnect(true);

      MySerial.println("WIFI CONNECTED");
      if (pendingSSID[0] != '\0') {
        saveWiFiConfig(pendingSSID, pendingPWD);
        pendingSSID[0] = '\0';
        pendingPWD[0] = '\0';
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      // 累计断线次数
      if (disconnected_num > 5) {
        WiFi.setAutoReconnect(false);
      } else {
        disconnected_num++;
      }

      MySerial.println("WIFI DISCONNECTED");
      int errorCode = 5;
      switch (info.wifi_sta_disconnected.reason)
      {
      case WIFI_REASON_NO_AP_FOUND:
        errorCode = 3;
        break;
      case WIFI_REASON_AUTH_FAIL:
      case WIFI_REASON_AUTH_EXPIRE:
        errorCode = 2;
        break;
      case WIFI_REASON_BEACON_TIMEOUT:
      case WIFI_REASON_ASSOC_FAIL:
      case WIFI_REASON_HANDSHAKE_TIMEOUT:
      case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        errorCode = 1;
        break;
      case WIFI_REASON_CONNECTION_FAIL:
        errorCode = 4;
        break;
      default:
        errorCode = 5;
        break;
      }
      MySerial.printf("+CWJAP:%d\r\n", errorCode);
      break;
    }

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      MySerial.println("WIFI GOT IP");
      break;

    default:
      break;
  }

  ATCWState();// 获取当前 Wi-Fi 状态
}

void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr) {
  preferences.begin(NVS_UART_NAMESPACE, false);
  preferences.putInt("baud", baud);
  preferences.putInt("data_bits", dataBits);
  preferences.putInt("stop_bits", stopBits);
  preferences.putInt("parity", parity);
  preferences.putInt("flow_control", addr);
  preferences.end();
}

bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr) {
  preferences.begin(NVS_UART_NAMESPACE, true);
  if (!preferences.isKey("baud") || !preferences.isKey("data_bits") || !preferences.isKey("stop_bits") || !preferences.isKey("parity")) {
    preferences.end();
    return false;
  }

  *baud = preferences.getInt("baud", 115200);
  *dataBits = preferences.getInt("data_bits", 8);
  *stopBits = preferences.getInt("stop_bits", 1);
  *parity = preferences.getInt("parity", 0);
  *addr = preferences.getInt("flow_control", 0);
  preferences.end();

  return true;
}

void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr) {
  MySerial.printf("+UART_DEF:%d,%d,%d,%d,%d\r\n", baud, dataBits, stopBits, parity, addr);
}

void saveWiFiConfig(char* ssid, char* pwd) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString("ssid", ssid);
  preferences.putString("pwd", pwd);
  preferences.end();
}

bool loadWiFiConfig(char* ssid, char* pwd) {
  preferences.begin(NVS_NAMESPACE, true);
  String savedSsid = preferences.getString("ssid", "");
  String savedPwd = preferences.getString("pwd", "");
  preferences.end();
  
  if (savedSsid.length() == 0) {
    return false;
  }
  
  strncpy(ssid, savedSsid.c_str(), 32);
  ssid[31] = '\0';
  strncpy(pwd, savedPwd.c_str(), 64);
  pwd[63] = '\0';
  
  return true;
}

void clearWiFiConfig() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.clear();
  preferences.end();
}

void ScanWiFi() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    MySerial.println("OK");
  } else {
    for (int i = 0; i < n; ++i) {
      char macStr[18];
      formatMacAddress(WiFi.BSSID(i), macStr);
      int ecn = getEcnValue(WiFi.encryptionType(i));
      MySerial.printf("+CWLAP:(%d,\"%s\",%d,\"%s\",%d,0,0,\"\",\"\",\"\",0)\r\n",
                    ecn,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    macStr,
                    WiFi.channel(i));
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    MySerial.println("OK");
  }
  WiFi.scanDelete();
}

void DoBLEScan(int duration) {
  ble_scan_lock = true;
  pBLEScan->stop();
  vTaskDelay(10 / portTICK_PERIOD_MS);
  // 设置扫描回调函数
  pBLEScan->setAdvertisedDeviceCallbacks(&bleScanCallback);
  pBLEScan->start(duration, false);

  // 等待扫描完成
  MySerial.println("+BLESCANDONE");
  ble_scan_lock = false;
}

bool autoConnect(char* ssid, char* pwd) {
  //自动连接WiFi，尝试WPA2和OPEN两种模式
  const wifi_auth_mode_t authModes[] = {
    WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_OPEN
  };

  for (int i = 0; i < 2; ++i) {
    WiFi.disconnect();
    WiFi.setMinSecurity(authModes[i]);
    WiFi.begin(ssid, pwd);

    const int timeout = 5000;
    const int interval = 200;
    int elapsed = 0;

    while (elapsed < timeout) {
      wl_status_t status = WiFi.status();

      if (status == WL_CONNECTED) {
        return true;
      }

      if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        break;
      }

      vTaskDelay(interval / portTICK_PERIOD_MS);
      elapsed += interval;
    }
  }

  return false;
}

void DoWiFiConnect(char* ssid, char* pwd) {
  if (strlen(ssid) == 0) {
    MySerial.println("ERROR");
    return;
  }

  strncpy(pendingSSID, ssid, sizeof(pendingSSID) - 1);
  pendingSSID[sizeof(pendingSSID) - 1] = '\0';
  strncpy(pendingPWD, pwd, sizeof(pendingPWD) - 1);
  pendingPWD[sizeof(pendingPWD) - 1] = '\0';

  WiFi.disconnect();
  // 重置断线次数
  disconnected_num = 0;
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pwd);
  MySerial.println("OK");
}

void processCommand(char* cmd) {
  // MySerial.println(cmd);//回显AT指令

  if (strcmp(cmd, "AT") == 0) {
    //测试
    MySerial.println("OK");
  } else if (strcmp(cmd, "AT+RESTART") == 0) {
    //重启
    MySerial.println("OK");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP.restart();
  } else if (strcmp(cmd, "AT+CWLAP") == 0) {
    //获取WiFi列表
    ScanWiFi();
  } else if (strncmp(cmd, "AT+CWJAP=", 9) == 0) {
    //连接WiFi
    char ssid[33];
    char pwd[65];
    
    if (parseWiFiCommand(cmd, ssid, pwd)) {
      DoWiFiConnect(ssid, pwd);
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, "AT+BLESCAN=", 11) == 0) {
    //扫描BLE
    int mode = 0;
    int duration = 0;
    
    if (parseBLECommand(cmd, &mode, &duration,&filter_type,filter_param)) {
      DoBLEScan(duration);
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, "AT+BLE_LST=", 11) == 0) {
    //保存BLE列表
    int count = 0;
    if (parseBleListCommand(cmd, &count)) {
      saveBleListConfig();
      sendBleListReport(count);
       bleCount = count;
      MySerial.println("OK");
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, "AT+UART_DEF=", 12) == 0) {
    //设置串口参数
    int baud = 0;
    int dataBits = 0;
    int stopBits = 0;
    int parity = 0;
    int addr = 0;

    if (parseUartConfigCommand(cmd, &baud, &dataBits, &stopBits, &parity, &addr)) {
      saveUartConfig(baud, dataBits, stopBits, parity, addr);
      sendUartConfigReport(baud, dataBits, stopBits, parity, addr);
      MySerial.println("OK");
    } else {
      MySerial.println("ERROR");
    }
  } else if (strcmp(cmd, "AT+CWSTATE?") == 0) {
    // 获取WiFi状态
    ATCWState();
  } else {
    // 无效指令
    return;
  }
}

// FreeRTOS任务函数，用于处理串口输入并将命令发送到队列
void DebugSerialTask(void* pvParameters) {
  char localBuffer[SERIAL_BUFFER_SIZE];
  int localIndex = 0;
  CommandMessage msg;

  while (true) {
    while (Serial.available() > 0) {
      char c = Serial.read();

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        if (localIndex > 0) {
          localBuffer[localIndex] = '\0';
          strncpy(msg.cmd, localBuffer, SERIAL_BUFFER_SIZE);
          if (commandQueue != NULL) {
            if (xQueueSend(commandQueue, &msg, 0) != pdTRUE) {
              Serial.println("ERROR: command queue full");
            }
          }
          localIndex = 0;
        }
      } else {
        if (localIndex < SERIAL_BUFFER_SIZE - 1) {
          localBuffer[localIndex++] = c;
        } else {
          localIndex = 0;
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// FreeRTOS任务函数，用于处理MySerial输入并将命令发送到队列
void MySerialTask(void* pvParameters) {
  char localBuffer[SERIAL_BUFFER_SIZE];
  int localIndex = 0;
  CommandMessage msg;

  while (true) {
    while (MySerial.available() > 0) {
      char c = MySerial.read();

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        if (localIndex > 0) {
          localBuffer[localIndex] = '\0';
          strncpy(msg.cmd, localBuffer, SERIAL_BUFFER_SIZE);
          if (commandQueue != NULL) {
            if (xQueueSend(commandQueue, &msg, 0) != pdTRUE) {
              MySerial.println("ERROR: command queue full");
            }
          }
          localIndex = 0;
        }
      } else {
        if (localIndex < SERIAL_BUFFER_SIZE - 1) {
          localBuffer[localIndex++] = c;
        } else {
          localIndex = 0;
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// FreeRTOS任务函数，用于从队列中接收命令并处理
void CommandTask(void* pvParameters) {
  // 启动Modbus TCP服务器
  ServerStart();

  int uartBaud = 115200;
  int uartDataBits = 8;
  int uartStopBits = 1;
  int uartParity = 0;
  int uartAddr = 0;

  if (loadUartConfig(&uartBaud, &uartDataBits, &uartStopBits, &uartParity, &uartAddr)) {
    sendUartConfigReport(uartBaud, uartDataBits, uartStopBits, uartParity, uartAddr);
  }

  if (loadBleListConfig()) {
    for (int i = 0; i < MAX_BLE_ADDRESSES; ++i) {
      if (bleMacs[i][0] != '\0') {
        bleCount++;
      }
    }
    if (bleCount > 0) {
      sendBleListReport(bleCount);
    }
  }

  char ssid[33] = "";
  char pwd[65] = "";

  if (loadWiFiConfig(ssid, pwd)) {
    autoConnect(ssid, pwd);
  }

  CommandMessage msg;
  while (true) {
    if (xQueueReceive(commandQueue, &msg, portMAX_DELAY) == pdTRUE) {
      processCommand(msg.cmd);
    }
  }
}

// FreeRTOS任务函数，用于处理传感器数据
void BLESensorTask(void* pvParameters) {
  while (true) {
    vTaskDelay(3000 / portTICK_PERIOD_MS); // 延迟3秒后开始处理传感器数据
    if (ble_scan_lock) continue; // 如果正在扫描BLE设备，则跳过本次循环
    // 设置回调函数
    pBLEScan->setAdvertisedDeviceCallbacks(&bleSensorCallback);
    pBLEScan->start(5, false); // 开始扫描5秒
    // 等待扫描完成
    uint8_t cwState = getATCWState();
    int8_t rssi = WiFi.RSSI();
    MySerial.printf("+SENSOR:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", bleSensorData[0].temp, bleSensorData[0].hum, bleSensorData[1].temp, bleSensorData[1].hum, bleSensorData[2].temp, bleSensorData[2].hum, bleSensorData[3].temp, bleSensorData[3].hum, bleSensorData[4].temp, bleSensorData[4].hum, bleSensorData[5].temp, bleSensorData[5].hum, bleSensorData[6].temp, bleSensorData[6].hum, bleSensorData[7].temp, bleSensorData[7].hum, bleSensorData[8].temp, bleSensorData[8].hum, bleSensorData[9].temp, bleSensorData[9].hum,cwState, rssi);
  }
}

// FreeRTOS任务函数，用于处理Modbus TCP连接
void ModbusTCPTask(void *pvParameters) {
  while (true) {
    ModbusTCPHandler();
  }
}

// 获取符合 ESP-AT 协议规范的 Wi-Fi 状态码 (0-4)
uint8_t getATCWState() {
    wl_status_t status = WiFi.status();
    
    // 1. 处理“已连接”状态（需要进一步判断 IP）
    if (status == WL_CONNECTED) {
        // 直接获取底层 IP 的原始 32 位数值，避免 String 内存开销
        uint32_t ipRaw = WiFi.localIP(); 
        
        // 如果 IP 不为 0，说明已获取到 IPv4 地址 -> 状态 2
        if (ipRaw != 0) {
            return 2; 
        }
        // IP 为 0，说明已连接上 AP 但尚未获取 IP -> 状态 1
        return 1; 
    }
    
    // 2. 处理“未初始化/空闲”状态 -> 状态 0
    if (status == WL_IDLE_STATUS) {
        return 0; 
    }
    
    // 3. 处理“正在连接”状态 -> 状态 3
    // 底层枚举中没有 WL_CONNECTING，WL_SCAN_COMPLETED 代表扫描完成准备连接
    if (status == WL_SCAN_COMPLETED) {
        return 3; 
    }
    
    // 4. 处理“断开/失败”状态 -> 状态 4
    // 根据底层枚举，精确匹配所有断开或失败的情况
    if (status == WL_DISCONNECTED || 
        status == WL_CONNECTION_LOST || 
        status == WL_CONNECT_FAILED || 
        status == WL_NO_SSID_AVAIL) {
        return 4; 
    }
    
    // 兜底：对于其他未知状态（如 WL_NO_SHIELD, WL_STOPPED），默认归类为断开状态
    return 4; 
}

void ATCWState() {
  // wifi状态
  uint8_t cwState = getATCWState();
  String ssid = WiFi.SSID();
  int8_t rssi = WiFi.RSSI();
  String ip = WiFi.localIP().toString();
  MySerial.printf("+CWSTATE:%d,\"%s\",%d,\"%s\"\r\n",
                  cwState,
                  ssid.c_str(),
                  rssi,
                  ip.c_str());
}
void sendBLESensorReport() {
  MySerial.println("+BLESENSOR:1");
}
void scanMode() {
  MySerial.println("+SCANMODE:1");
}
int findBleDevice(const char* addr) {
  for (int i = 0; i < bleCount; ++i) {
    if (bleMacs[i][0] != '\0' && strcmp(bleMacs[i], addr) == 0) {
      return i;// 找到匹配的设备，返回索引
    }
  }
  return -1; // 未找到匹配的设备，返回-1
}

void setupEntry() {
  Serial.begin(115200);
  MySerial.begin(115200, SERIAL_8N1, 6, 7); // RX, TX
  int uartBaud = 115200;
  int uartDataBits = 8;
  int uartStopBits = 1;
  char uartParity = 'N';

  Serial.print("Arduino Core Version: "); 
  Serial.println(ESP_ARDUINO_VERSION_STR);// 打印 Arduino Core 版本
  Serial.print("ESP-IDF Version: ");
  Serial.println(esp_get_idf_version());// 打印 ESP-IDF 版本
  Serial.print("ESP32 Chip ID: ");
  Serial.println(ESP.getEfuseMac());
  Serial.print("ESP32 Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("ESP32 Chip Cores: ");
  Serial.println(ESP.getChipCores());

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  MySerial.println("ready");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleScanCallback);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.STA.begin();

  commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(CommandMessage));
  if (commandQueue == NULL) {
    MySerial.println("ERROR: command queue create failed");
  } else {
    xTaskCreate(DebugSerialTask, "DebugSerialTask", 4096, NULL, 1, NULL);
    xTaskCreate(MySerialTask, "MySerialTask", 4096, NULL, 1, NULL);
    xTaskCreate(BLESensorTask, "BLESensorTask", 4096, NULL, 1, NULL);
    xTaskCreate(ModbusTCPTask, "ModbusTCPTask", 4096, NULL, 1, NULL);
    xTaskCreate(CommandTask, "CommandTask", 8192, NULL, 1, NULL);
  }
}

void loopEntry() {
  taskYIELD();
}
