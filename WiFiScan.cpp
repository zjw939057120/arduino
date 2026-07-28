#include "WiFiScan.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "ModbusServer.h"
#include "HttpServer.h"
#include "MQTTSubClient.h"

// 串口缓冲区大小
#define SERIAL_BUFFER_SIZE 255
// WiFi配置命名空间
#define NVS_WIFI_NAMESPACE "wifi_config"
// 设备配置命名空间
#define NVS_DEVICE_NAMESPACE "device_config"
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
// 设置设备配置指令
#define AT_CMD_DEVICE_DEF "AT+DEVICE_DEF="
// 获取设备配置指令
#define AT_CMD_DEVICE_DEF_GET "AT+DEVICE_DEF?"

// 任务句柄
TaskHandles taskHandles = {NULL,NULL, NULL,NULL, NULL, NULL};
// WiFi配置
WiFiConfig wifiConfig;
// 设备配置
DeviceConfig deviceConfig;
// 设备状态
DeviceStatus deviceStatus = {false, false, 0};
// 串口配置
UartConfig uartConfig = {9600, 8, 1, 0, 1};
// 串口
HardwareSerial MySerial(1);

typedef struct {
  char cmd[SERIAL_BUFFER_SIZE];
} CommandMessage;

char serialBuffer[SERIAL_BUFFER_SIZE];
Preferences preferences;
BLEScan* pBLEScan;
static QueueHandle_t commandQueue = NULL;

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
    // MySerial.printf("+BLESCAN:\"%s\",%d,%s,%s,%s,%d\r\n", addr.c_str(), rssi, advDataStr, serviceData.c_str(), serviceUUID.c_str(), addrType);
    MySerial.printf("+BLESCAN:\"%s\",%d,%s,%d\r\n", addr.c_str(), rssi, advDataStr, addrType);
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
    // 温度数据
    bleSensorData[index].temp = ((uint8_t)advData[advLen - 3] << 8) | (uint8_t)advData[advLen - 4];
    // 湿度数据
    bleSensorData[index].hum = ((uint8_t)advData[advLen - 1] << 8) | (uint8_t)advData[advLen - 2];
  }
};

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
  char* paramStart = cmd + strlen(AT_CMD_CWJAP);
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
    char* paramStart = cmd + strlen(AT_CMD_BLE_SCAN);
    
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
  char* paramStart = cmd + strlen(AT_CMD_UART_DEF);
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
  if (*dataBits < 5 || *dataBits > 9) return false; // 5bit,6bit,7bit,8bit,9bit
  if (*stopBits < 1 || *stopBits > 3) return false; // 1=1bit,2=1.5bit,3=2bit
  if (*parity < 0 || *parity > 2) return false; // 0=None,1=Odd,2=Even
  if (*addr < 0 || *addr > 255) return false;

  return true;
}

bool parseSensorCommand(char* cmd, SensorData* data) {
  char* paramStart = cmd + strlen(AT_CMD_SENSOR); // skip "AT+SENSOR="

  // 解析 CO2
  char* comma = strchr(paramStart, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->CO2 = atoi(paramStart);

  // 解析 CH2O
  char* token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->CH2O = atoi(token);

  // 解析 TVOC
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->TVOC = atoi(token);
  
  // 解析 PM25
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->PM25 = atoi(token);
  
  // 解析 PM100
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->PM100 = atoi(token);

  // 解析 TEMP
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->TEMP = atoi(token);

  // 解析 RH
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->RH = atoi(token);

  // 解析 PM10
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *comma = '\0';
  data->PM10 = atoi(token);

  // 解析 TYPE
  token = comma + 1;
  data->TYPE = atoi(token);

  // Serial.println(String(data->CO2) + "," + String(data->CH2O) + "," + String(data->TVOC) + "," + 
  //                String(data->PM25) + "," + String(data->PM100) + "," + String(data->TEMP) + ","  + 
  //                String(data->RH) + "," + String(data->PM10) + ","  + String(data->TYPE));
   return true;
}

bool parseBleListCommand(char* cmd, int* count) {
  char* paramStart = cmd + strlen(AT_CMD_BLE_LST); // skip "AT+BLE_LST="
  
  // Parse first parameter: count
  char* comma = strchr(paramStart, ',');
  if (comma == NULL) {
    return false;
  }
  *comma = '\0';
  *count = atoi(paramStart);
  
  if (*count == 0) {
    return true; // 0 表示清空列表
  }
  else if (*count < 0 || *count > MAX_BLE_ADDRESSES) {
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

    strncpy(bleDevice[macIdx], start, 17);
    bleDevice[macIdx][17] = '\0';
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

bool parseMQTTCommand(char* cmd, char* ip, int* port, char* username, char* password){
  char* token = cmd + strlen(AT_CMD_MQTT_DEF); // skip "AT+MQTT_DEF="
  // 解析 IP
  char* comma = strchr(token, ',');
  if (comma == NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    *(comma - 1) = '\0';
  }else {
    *comma = '\0';
  }
  strncpy(ip, token, 15);
  ip[15] = '\0';
  // 解析 PORT
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  *port = atoi(token);
  // 解析 USERNAME
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    *(comma - 1) = '\0';
  }else {
    *comma = '\0';
  }
  strncpy(username, token, 15);
  username[15] = '\0';
  // 解析 PASSWORD
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma != NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    char* end = token + strlen(token);
    *(end - 1) = '\0';
  }
  strncpy(password, token, 15);
  password[15] = '\0';
  return true;
}

void saveMQTTConfig(char* ip, int port, char* username, char* password) {
  preferences.begin(NVS_MQTT_NAMESPACE, false);
  if (!preferences.getString("ip", "").equals(ip)) {
    preferences.putString("ip", ip);
  }
  if (preferences.getInt("port", 0) != port) {
    preferences.putInt("port", port);
  }
  if (!preferences.getString("username", "").equals(username)) {
    preferences.putString("username", username);
  }
  if (!preferences.getString("password", "").equals(password)) {
    preferences.putString("password", password);
  }
  preferences.end();

  // 更新MQTT配置
  strcpy(mqttConfig.ip, ip);
  mqttConfig.port = port;
  strcpy(mqttConfig.username, username);
  strcpy(mqttConfig.password, password);
}

void saveBleListConfig() {
  preferences.begin(NVS_BLE_NAMESPACE, false);
  if (preferences.getInt("count", 0) != bleCount) {
    preferences.putInt("count", bleCount);
  }
  String tmp = "";
  for (int i = 0; i < bleCount; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mac_%d", i);
    tmp = preferences.getString(key, "");
    if (!tmp.equals(bleDevice[i])) {
    preferences.putString(key, bleDevice[i]);
    }
  }
  preferences.end();
}

bool loadBleListConfig() {
  preferences.begin(NVS_BLE_NAMESPACE, true);
  bleCount = preferences.getInt("count", 0);
  for (int i = 0; i < bleCount; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mac_%d", i);
    String value = preferences.getString(key, "");
    if (value.length() > 0) {
      strncpy(bleDevice[i], value.c_str(), 17);
      bleDevice[i][17] = '\0';
    }
  }
  preferences.end();
  return true;
}

void sendBleListReport(int count) {
  MySerial.printf("+BLE_LST:%d", count);
  for (int i = 0; i < count; ++i) {
    if (bleDevice[i][0] != '\0') {
      MySerial.printf(",\"%s\"", bleDevice[i]);
    } else {
      MySerial.print(",\"\"");
    }
  }
  MySerial.println();
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
    //禁用自动重连
    WiFi.setAutoReconnect(false);
    MySerial.println(F("WIFI CONNECTED"));
    saveWiFiConfig(wifiConfig.ssid, wifiConfig.pwd);
  }
  break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
  {
    MySerial.println(F("WIFI DISCONNECTED"));
    int errorCode = 5;
    switch (info.wifi_sta_disconnected.reason) {
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
  }
  break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      MySerial.println(F("WIFI GOT IP"));
      break;
    default:
      break;
    }

  sendCWStateReport();// 获取当前 Wi-Fi 状态
}

void doSaveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr) {
  // 保存UART配置
  saveUartConfig(baud, dataBits, stopBits, parity, addr);
  // 发送配置报告
  sendUartConfigReport(baud, dataBits, stopBits, parity, addr);
}

void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr) {
  preferences.begin(NVS_UART_NAMESPACE, false);
  if (preferences.getInt("baud", 9600) != baud) {
    preferences.putInt("baud", baud);// 保存波特率
  }
  if (preferences.getInt("data_bits", 8) != dataBits) {
    preferences.putInt("data_bits", dataBits);// 保存数据位
  }
  if (preferences.getInt("stop_bits", 1) != stopBits) {
    preferences.putInt("stop_bits", stopBits);// 保存停止位
  }
  if (preferences.getInt("parity", 0) != parity) {
    preferences.putInt("parity", parity);// 保存校验位
  }
  if (preferences.getInt("addr", 1) != addr) {
    preferences.putInt("addr", addr);// 保存地址
  }
  preferences.end();
}

bool loadMQTTConfig(char* ip, int* port, char* username, char* password) {
  preferences.begin(NVS_MQTT_NAMESPACE, true);
  String savedIp = preferences.getString("ip", "");
  int savedPort = preferences.getInt("port", 0);
  String savedUsername = preferences.getString("username", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();
  if (savedIp.length() == 0 || savedPort == 0 || savedUsername.length() == 0 || savedPassword.length() == 0) {
    return false;
  }
  strncpy(ip, savedIp.c_str(), 16);
  ip[15] = '\0';
  *port = savedPort;
  strncpy(username, savedUsername.c_str(), 16);
  username[15] = '\0';
  strncpy(password, savedPassword.c_str(), 16);
  password[15] = '\0';
  return true;
}

void sendMQTTConfigReport(char* ip, int port, char* username, char* password){
  MySerial.printf("+MQTT_DEF:\"%s\",%d,\"%s\",\"%s\"\r\n", ip, port, username, password);
}

bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr) {
  preferences.begin(NVS_UART_NAMESPACE, true);
  *baud = preferences.getInt("baud", 9600);// 读取波特率
  *dataBits = preferences.getInt("data_bits", 8);// 读取数据位
  *stopBits = preferences.getInt("stop_bits", 1);// 读取停止位
  *parity = preferences.getInt("parity", 0);// 读取校验位
  *addr = preferences.getInt("addr", 1);// 读取地址
  preferences.end();

  return true;
}

void sendUartConfigReport(int baud, int dataBits, int stopBits, int parity, int addr) {
  MySerial.printf("+UART_DEF:%d,%d,%d,%d,%d\r\n", baud, dataBits, stopBits, parity, addr);
}

void saveWiFiConfig(char* ssid, char* pwd) {
  preferences.begin(NVS_WIFI_NAMESPACE, false);
  String tmp = "";
  tmp = preferences.getString("ssid", "");
  if (!tmp.equals(ssid)) {
    preferences.putString("ssid", ssid);
  }
  tmp = preferences.getString("pwd", "");
  if (!tmp.equals(pwd)) {
    preferences.putString("pwd", pwd);
  }
  preferences.end();
}

bool loadWiFiConfig(char* ssid, char* pwd) {
  preferences.begin(NVS_WIFI_NAMESPACE, true);
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

bool parseDeviceConfigCommand(char* cmd, char* local_ip, char* gateway_ip, char* subnet_mask, char* dns_ip){
  char* token = cmd + strlen(AT_CMD_DEVICE_DEF); // skip "AT+DEVICE_DEF="
  // 解析 LOCAL_IP
  char* comma = strchr(token, ',');
  if (comma == NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    *(comma - 1) = '\0';
  }else {
    *comma = '\0';
  }
  strncpy(local_ip, token, 15);
  local_ip[15] = '\0';
  // 解析 GATEWAY_IP
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    *(comma - 1) = '\0';
  }else {
    *comma = '\0';
  }
  strncpy(gateway_ip, token, 15);
  gateway_ip[15] = '\0';
  // 解析 SUBNET_MASK
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma == NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    *(comma - 1) = '\0';
  }else {
    *comma = '\0';
  }
  strncpy(subnet_mask, token, 15);
  subnet_mask[15] = '\0';
  // 解析 DNS_IP
  token = comma + 1;
  comma = strchr(token, ',');
  if (comma != NULL) return false;
  else if(*token == '"') {
    // 跳过双引号
    token++;
    char* end = token + strlen(token);
    *(end - 1) = '\0';
  }
  strncpy(dns_ip, token, 15);
  dns_ip[15] = '\0';
  return true;
}

void doSaveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip) {
  saveDeviceConfig(local_ip, gateway_ip, subnet_mask, dns_ip);
  sendDeviceConfigReport(local_ip, gateway_ip, subnet_mask, dns_ip);

  WiFi.disconnect();
  // 设置设备信息
  configStation();
  // 连接WiFi
  wifiConnect();
  // 重置WiFi检查次数
  deviceStatus.wifi_check_count = 0;
}

void saveDeviceConfig(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip){
  preferences.begin(NVS_DEVICE_NAMESPACE, false);
  if (!preferences.getString("local_ip", "").equals(local_ip))
    deviceConfig.local_ip, preferences.putString("local_ip", local_ip);
    
  if (!preferences.getString("gateway_ip", "").equals(gateway_ip))
    deviceConfig.gateway_ip, preferences.putString("gateway_ip", gateway_ip);

  if (!preferences.getString("subnet_mask", "").equals(subnet_mask))
    deviceConfig.subnet_mask, preferences.putString("subnet_mask", subnet_mask);

  if (!preferences.getString("dns_ip", "").equals(dns_ip))
    deviceConfig.dns_ip, preferences.putString("dns_ip", dns_ip);
  preferences.end();

  strcpy(deviceConfig.local_ip, local_ip);
  strcpy(deviceConfig.gateway_ip, gateway_ip);
  strcpy(deviceConfig.subnet_mask, subnet_mask);
  strcpy(deviceConfig.dns_ip, dns_ip);
}

void sendDeviceConfigReport(const char* local_ip, const char* gateway_ip, const char* subnet_mask, const char* dns_ip){
  MySerial.printf("+DEVICE_DEF:\"%s\",\"%s\",\"%s\",\"%s\"\r\n", local_ip, gateway_ip, subnet_mask, dns_ip);
}

void loadDeviceConfig() {
  // 加载设备配置
  preferences.begin(NVS_DEVICE_NAMESPACE, true);
  // 加载设备IP地址
  strncpy(deviceConfig.local_ip, preferences.getString("local_ip", "").c_str(), 16);
  deviceConfig.local_ip[15] = '\0';
  // 加载网关IP地址
  strncpy(deviceConfig.gateway_ip, preferences.getString("gateway_ip", "").c_str(), 16);
  deviceConfig.gateway_ip[15] = '\0';
  // 加载子网掩码
  strncpy(deviceConfig.subnet_mask, preferences.getString("subnet_mask", "").c_str(), 16);
  deviceConfig.subnet_mask[15] = '\0';
  // 加载DNS服务器IP地址
  strncpy(deviceConfig.dns_ip, preferences.getString("dns_ip", "").c_str(), 16);
  deviceConfig.dns_ip[15] = '\0';
  preferences.end();

  // 加载AP配置
  // 获取设备名称
  getHostname(deviceConfig.ap_ssid);
  // 设置默认密码
  strcpy(deviceConfig.ap_pwd, "12345678");
  // 获取设备地址
  getMacAddress(deviceConfig.mac);
}


void configStation(){
  // 设置设备名称
  WiFi.hostname(deviceConfig.ap_ssid);
  // 配置静态IP地址
  if (strcmp(deviceConfig.local_ip, "") != 0 && strcmp(deviceConfig.gateway_ip, "") != 0 && strcmp(deviceConfig.subnet_mask, "") != 0 && strcmp(deviceConfig.dns_ip, "") != 0) {
    WiFi.config(IPAddress(deviceConfig.local_ip), IPAddress(deviceConfig.gateway_ip), IPAddress(deviceConfig.subnet_mask), IPAddress(deviceConfig.dns_ip));
  }
}

void wifiConnect() {
  // 连接WiFi
  WiFi.begin(wifiConfig.ssid, strcmp(wifiConfig.pwd, "") == 0 ? NULL : wifiConfig.pwd);
}

void restore() {
  preferences.begin(NVS_WIFI_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  preferences.begin(NVS_DEVICE_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  preferences.begin(NVS_UART_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  preferences.begin(NVS_BLE_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  // 重启ESP32
  delay(1000);
  ESP.restart();
}

void sendCWJAPReport() {
  MySerial.printf("+CWJAP:\"%s\",\"%s\"\r\n", wifiConfig.ssid, wifiConfig.pwd);
}

void SendScanWiFiReport() {
  deviceStatus.wifi_scaning = true;
  int n = WiFi.scanNetworks();
  if (n == 0) {
    MySerial.println("OK");
  } else {
    uint8_t sum = 0;
    for (int i = 0; i < n; ++i) {
      // 过滤非ASCII字符
      if (containsNonASCII(WiFi.SSID(i).c_str())) {
        continue;
      }
      if (sum > 20) {
        // 最多扫描20个网络
        return;
      }
      
      char macStr[18];
      formatMacAddress(WiFi.BSSID(i), macStr);
      int ecn = getEcnValue(WiFi.encryptionType(i));
      MySerial.printf("+CWLAP:(%d,\"%s\",%d,\"%s\",%d,0,0,\"\",\"\",\"\",0)\r\n",
                    ecn,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    macStr,
                    WiFi.channel(i));
      MySerial.flush();
      sum++;
    }
    MySerial.println("OK");
  }
  WiFi.scanDelete();
  deviceStatus.wifi_scaning = false;
}

void ScanWiFiHandler(char* content, int size) {
  deviceStatus.wifi_scaning = true;
  
  // 使用单字符键名：e=ecn, s=ssid, r=rssi, m=mac, c=ch
  int offset = snprintf(content, size, "{\"code\":0,\"msg\":\"OK\",\"data\":[");
  
  int n = WiFi.scanNetworks();
  uint8_t sum = 0;
  
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      // 过滤非ASCII字符
      if (containsNonASCII(WiFi.SSID(i).c_str())) {
        continue;
      }
      // 最多扫描12个网络
      if (sum >= 12) {
        break; 
      }
      
      char macStr[18];
      formatMacAddress(WiFi.BSSID(i), macStr);
      int ecn = getEcnValue(WiFi.encryptionType(i));
      
      // 如果不是第一个元素，先追加逗号
      if (sum > 0) {
        offset += snprintf(content + offset, size - offset, ",");
      }
      
      // 拼接单个网络对象的 JSON（使用单字符键名）
      offset += snprintf(content + offset, size - offset, 
                         "{\"e\":%d,\"s\":\"%s\",\"r\":%d,\"m\":\"%s\",\"c\":%d}", 
                         ecn, 
                         WiFi.SSID(i).c_str(), 
                         WiFi.RSSI(i), 
                         macStr, 
                         WiFi.channel(i));
      
      sum++;
    }
  }
  
  // 闭合 data 数组和 JSON 对象
  snprintf(content + offset, size - offset, "]}");
  
  WiFi.scanDelete();
  deviceStatus.wifi_scaning = false;
}

void DoBLEScan(int duration) {
  deviceStatus.ble_scaning = true;
  pBLEScan->stop();
  delay(10);
  // 设置扫描回调函数
  pBLEScan->setAdvertisedDeviceCallbacks(&bleScanCallback);
  pBLEScan->start(duration, false);

  // 等待扫描完成
  MySerial.println("+BLESCANDONE");
  deviceStatus.ble_scaning = false;
}

void DoWiFiConnect(const char* ssid, const char* pwd) {
  if (strlen(ssid) == 0) {
    MySerial.println("ERROR");
    return;
  }

  strncpy(wifiConfig.ssid, ssid, sizeof(wifiConfig.ssid) - 1);
  wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';
  strncpy(wifiConfig.pwd, pwd, sizeof(wifiConfig.pwd) - 1);
  wifiConfig.pwd[sizeof(wifiConfig.pwd) - 1] = '\0';

  // 设置设备信息
  configStation();
  // 连接WiFi
  wifiConnect();
  // 重置WiFi检查次数
  deviceStatus.wifi_check_count = 0;
  MySerial.println("OK");

  Serial.print("connect ssid: ");
  Serial.print(wifiConfig.ssid);
  Serial.print(" pwd: ");
  Serial.println(wifiConfig.pwd);
}

void processCommand(char* cmd) {
  // 打印命令
  // Serial.println(cmd);

  if (strcmp(cmd, AT_CMD_AT) == 0) {
    //测试
    MySerial.println(F("OK"));
  } else if (strcmp(cmd, AT_CMD_RESTART) == 0) {
    MySerial.println(F("OK"));
    // 重启ESP32
    delay(1000);
    ESP.restart();
  } else if (strcmp(cmd, AT_CMD_CWLWAP) == 0) {
    // 获取WiFi列表
    SendScanWiFiReport();
  } else if (strncmp(cmd, AT_CMD_CWJAP, strlen(AT_CMD_CWJAP)) == 0) {
    //连接WiFi
    char ssid[33];
    char pwd[65];
    
    if (parseWiFiCommand(cmd, ssid, pwd)) {
      DoWiFiConnect(ssid, pwd);
    } else {
      // 连接WiFi错误
      MySerial.println(F("ERROR"));
    }
  } else if (strcmp(cmd, AT_CMD_CWJAP_GET) == 0) {
    // 获取连接WiFi信息
    sendCWJAPReport();
  } else if (strncmp(cmd, AT_CMD_BLE_SCAN, strlen(AT_CMD_BLE_SCAN)) == 0) {
    //扫描BLE
    int mode = 0;
    int duration = 0;
    
    if (parseBLECommand(cmd, &mode, &duration,&filter_type,filter_param)) {
      DoBLEScan(duration);
    } else {
      // BLE扫描错误
      MySerial.println(F("ERROR"));
    }
  } else if (strncmp(cmd, AT_CMD_BLE_LST, strlen(AT_CMD_BLE_LST)) == 0) {
    //保存BLE列表
    int count = 0;
    if (parseBleListCommand(cmd, &count)) {
      bleCount = count;
      saveBleListConfig();
      sendBleListReport(count);
    } else {
      // BLE列表错误
      MySerial.println(F("ERROR"));
    }
  } else if (strcmp(cmd, AT_CMD_BLE_LST_GET) == 0) {
    // 获取BLE列表
    sendBleListReport(bleCount);
  } else if (strncmp(cmd, AT_CMD_UART_DEF, strlen(AT_CMD_UART_DEF)) == 0) {
    //设置串口参数
    int baud = 0;
    int dataBits = 0;
    int stopBits = 0;
    int parity = 0;
    int addr = 0;

    if (parseUartConfigCommand(cmd, &baud, &dataBits, &stopBits, &parity, &addr)) {
      saveUartConfig(baud, dataBits, stopBits, parity, addr);
      uartConfig.baud = baud;
      uartConfig.dataBits = dataBits;
      uartConfig.stopBits = stopBits;
      uartConfig.parity = parity;
      uartConfig.addr = addr;
      sendUartConfigReport(uartConfig.baud, uartConfig.dataBits, uartConfig.stopBits, uartConfig.parity, uartConfig.addr);
    } else {
      // 串口配置错误
      MySerial.println(F("ERROR"));
    }
  } else if (strcmp(cmd, AT_CMD_UART_DEF_GET) == 0) {
    // 获取串口参数
    sendUartConfigReport(uartConfig.baud, uartConfig.dataBits, uartConfig.stopBits, uartConfig.parity, uartConfig.addr);
  } else if (strcmp(cmd, AT_CMD_CWSTATE_GET) == 0) {
    // 获取WiFi状态
    sendCWStateReport();
  } else if (strncmp(cmd, AT_CMD_SENSOR, strlen(AT_CMD_SENSOR)) == 0) {
    // 传感器数据
    parseSensorCommand(cmd, &sensorData);
  } else if (strncmp(cmd, AT_CMD_MQTT_DEF, strlen(AT_CMD_MQTT_DEF)) == 0) {
    // MQTT服务配置
    char ip[16];
    int port = 0;
    char username[16];
    char password[16];
    if (parseMQTTCommand(cmd, ip, &port, username, password)) {
      saveMQTTConfig(ip, port, username, password);
      sendMQTTConfigReport(ip, port, username, password);
      // 重新连接MQTT服务器
      MQTTSubClientReConnect();
    } else {
      // MQTT配置错误
      MySerial.println(F("ERROR"));
    }
  } else if (strcmp(cmd, AT_CMD_MQTT_DEF_GET) == 0) {
    // 获取MQTT参数
    sendMQTTConfigReport(mqttConfig.ip, mqttConfig.port, mqttConfig.username, mqttConfig.password);
  } else if (strncmp(cmd, AT_CMD_DEVICE_DEF, strlen(AT_CMD_DEVICE_DEF)) == 0) {
    // 设备配置
    char local_ip[16];
    char gateway_ip[16];
    char subnet_mask[16];
    char dns_ip[16];
    if (parseDeviceConfigCommand(cmd, local_ip, gateway_ip, subnet_mask, dns_ip)) {
      doSaveDeviceConfig(local_ip, gateway_ip, subnet_mask, dns_ip);
    } else {
      // 设备配置错误
      MySerial.println(F("ERROR"));
    }

   } else if (strcmp(cmd, AT_CMD_DEVICE_DEF_GET) == 0) {
    // 获取设备参数
    sendDeviceConfigReport(deviceConfig.local_ip, deviceConfig.gateway_ip, deviceConfig.subnet_mask, deviceConfig.dns_ip);
   } else {  
    // 无效指令
    Serial.print(F("ERROR:"));
    Serial.println(cmd);
    return;
  }
}

// DebugSerial任务函数，用于处理串口输入并将命令发送到队列
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
              Serial.println("ERROR: queue full");
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

    delay(10);
  }
}

// MySerial任务函数，用于处理MySerial输入并将命令发送到队列
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
              Serial.println("ERROR: queue full");
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

    delay(10);
  }
}

// BLE传感器任务函数，用于处理传感器数据
void BLESensorTask(void* pvParameters) {
  while (true) {
    delay(5000);// 每5秒处理一次传感器数据
    // 如果正在扫描BLE设备，则跳过本次循环
    if (deviceStatus.ble_scaning) {
      continue;
    }
    // 如果有配置BLE传感器，则开始扫描
    else if (bleCount > 0) {
      pBLEScan->setAdvertisedDeviceCallbacks(&bleSensorCallback);
      pBLEScan->start(5, false); // 开始扫描5秒
    }
    sendBleSensorReport();
  }
}

// Modbus TCP任务函数，用于处理Modbus TCP连接
void ModbusServerTask(void *pvParameters) {
  ModbusServerStart();
  while (true) {
    ModbusServerHandler();
  }
}

// HTTP任务函数，用于处理HTTP连接
void HttpServerTask(void *pvParameters) {
  HttpServerStart();
  while (true) {
    HttpServerHandler();
  }
}

// MQTT任务函数，用于处理MQTT连接
void MQTTSubClientTask(void *pvParameters) {
  MQTTSubClientStart();
  while (true) {
    MQTTSubClientHandler();
  }
}

// 命令任务函数，用于从队列中接收命令并处理
void CommandTask(void* pvParameters) {
  CommandMessage msg;
  while (true) {
    if (xQueueReceive(commandQueue, &msg, portMAX_DELAY) == pdTRUE) {
      processCommand(msg.cmd);
    }
  }
}

// 网络任务函数，用于处理网络状态检查
void NetworkTask(void* pvParameters) {
  // 上报WiFi状态
  sendCWStateReport();
  while (true) {
    // 每10秒检查一次网络状态
    delay(10000);
    if(WiFi.status() != WL_CONNECTED) {
      if(deviceStatus.wifi_scaning) {
        continue;
      } else if(deviceStatus.wifi_check_count >= 4) {
        // 连接失败超过4次，认为连接失败
        continue;
      }
      // 设置设备信息
      configStation();
      // 连接WiFi
      wifiConnect();
      deviceStatus.wifi_check_count++;
    }
  }
}

// 其他任务函数，用于处理其他任务，如关闭AP模式
void MiscTask(void* pvParameters) {
  delay(10 * 60 * 1000); // 10分钟后关闭AP模式
  // 切换到STA模式
  WiFi.mode(WIFI_STA);
  // 设置设备信息
  configStation();
  // 连接WiFi
  wifiConnect();
  // 重置WiFi检查次数
  deviceStatus.wifi_check_count = 0;
  // 删除HTTP任务
  vTaskDelete(taskHandles.httpServerTaskHandle);
  taskHandles.httpServerTaskHandle = NULL;

  while (true) {
    delay(10 * 60 * 1000);// 每10分钟检查一次网络状态
    // 重置WiFi检查次数
    deviceStatus.wifi_check_count = 0;
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

void sendCWStateReport() {
  // wifi状态
  uint8_t cwState = getATCWState();
  MySerial.printf("+CWSTATE:%d,\"%s\",%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\r\n",
                  cwState,
                  WiFi.SSID().c_str(),
                  WiFi.RSSI(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.subnetMask().toString().c_str(),
                  deviceConfig.mac,
                  WiFi.dnsIP().toString().c_str()
                );
  MySerial.flush();
}

int findBleDevice(const char* addr) {
  for (int i = 0; i < bleCount; ++i) {
    if (bleDevice[i][0] != '\0' && strcmp(bleDevice[i], addr) == 0) {
      return i;// 找到匹配的设备，返回索引
    }
  }
  return -1; // 未找到匹配的设备，返回-1
}

int sendBleSensorReport() {
  MySerial.flush();
  // 初始化未检测到的传感器数据为0
  for (int i = bleCount; i < MAX_BLE_ADDRESSES; i++) {
    bleSensorData[i].temp = 0;
    bleSensorData[i].hum = 0;
  }

  uint8_t cwState = getATCWState();
  int8_t rssi = WiFi.RSSI();
  // 发送BLE传感器数据
  MySerial.printf("+BLE_SENSOR:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", bleCount, bleSensorData[0].temp, bleSensorData[0].hum, bleSensorData[1].temp, bleSensorData[1].hum, bleSensorData[2].temp, bleSensorData[2].hum, bleSensorData[3].temp, bleSensorData[3].hum, bleSensorData[4].temp, bleSensorData[4].hum, bleSensorData[5].temp, bleSensorData[5].hum, bleSensorData[6].temp, bleSensorData[6].hum, bleSensorData[7].temp, bleSensorData[7].hum, bleSensorData[8].temp, bleSensorData[8].hum, bleSensorData[9].temp, bleSensorData[9].hum, cwState, rssi);
  return bleCount;
}

bool containsNonASCII(const char* ssid) {
  if (ssid == NULL) {
    return false;
  }
  for (int i = 0; ssid[i] != '\0'; i++) {
    if (ssid[i] < 0x20 || ssid[i] > 0x7E) {
      return true;
    }
  }
  return false;
}
void getMacAddress(char *macStr) {
  uint64_t chipId = ESP.getEfuseMac();
  // 将 64位整数按字节拆分，并强制按照网络标准的大端序（从高位到低位）格式化输出
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(chipId), // 最低位字节对应 MAC 地址的第一个字节
           (uint8_t)(chipId >> 8),
           (uint8_t)(chipId >> 16),
           (uint8_t)(chipId >> 24),
           (uint8_t)(chipId >> 32),
           (uint8_t)(chipId >> 40)); // 最高位字节对应 MAC 地址的最后一个字节
}
void getHostname(char *hostnameStr) {
  uint64_t chipId = ESP.getEfuseMac();
  // 将 64位整数按字节拆分，并强制按照网络标准的大端序（从高位到低位）格式化输出
  sprintf(hostnameStr, "%02X%02X%02X%02X%02X%02X",
           (uint8_t)(chipId), // 最低位字节对应 MAC 地址的第一个字节
           (uint8_t)(chipId >> 8),
           (uint8_t)(chipId >> 16),
           (uint8_t)(chipId >> 24),
           (uint8_t)(chipId >> 32),
           (uint8_t)(chipId >> 40)); // 最高位字节对应 MAC 地址的最后一个字节
}
void setupEntry() {
  Serial.begin(115200);
  MySerial.begin(115200, SERIAL_8N1, 6, 7); // RX, TX

  Serial.print("Arduino Core Version: "); 
  Serial.println(ESP_ARDUINO_VERSION_STR);// 打印 Arduino Core 版本
  Serial.print("ESP-IDF Version: ");
  Serial.println(esp_get_idf_version());// 打印 ESP-IDF 版本
  Serial.print("ESP32 Chip ID: ");
  Serial.println(ESP.getEfuseMac());
  Serial.print("ESP32 Chip Model: ");
  Serial.println(ESP.getChipModel());
  MySerial.println("ready");

  // 加载设备配置
  loadDeviceConfig();
  // 加载WiFi配置
  loadWiFiConfig(wifiConfig.ssid, wifiConfig.pwd);
  // 加载UART配置
  int baud = 0;
  int dataBits = 0;
  int stopBits = 0;
  int parity = 0;
  int addr = 0;
  if (loadUartConfig(&baud, &dataBits, &stopBits, &parity, &addr)) {
    uartConfig.baud = baud;
    uartConfig.dataBits = dataBits;
    uartConfig.stopBits = stopBits;
    uartConfig.parity = parity;
    uartConfig.addr = addr;
    sendUartConfigReport(uartConfig.baud, uartConfig.dataBits, uartConfig.stopBits, uartConfig.parity, uartConfig.addr);
  }
  // 加载BLE列表配置
  if (loadBleListConfig()) {
    sendBleListReport(bleCount);
  }

  // 初始化WiFi 设备
  WiFi.disconnect();
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  // 禁用自动重连
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(WiFiEvent);
  // 设置设备信息
  configStation();
  // 连接WiFi
  wifiConnect();
  // 重置WiFi检查次数
  deviceStatus.wifi_check_count = 0;

  // 创建AP模式
  WiFi.AP.create(deviceConfig.ap_ssid, deviceConfig.ap_pwd);
  // 启动AP模式
  WiFi.AP.begin();

  // 初始化BLE设备
  BLEDevice::init(deviceConfig.ap_ssid);
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleScanCallback);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  // 创建命令队列
  commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(CommandMessage));
  if (commandQueue == NULL) {
    Serial.println("ERROR: queue create failed");
  } else {
    // 创建任务
    // 调试串口任务
    xTaskCreate(DebugSerialTask, "DebugSerial", 4096, NULL, 1, &taskHandles.debugSerialTaskHandle);
    // 串口任务
    xTaskCreate(MySerialTask, "MySerial", 4096, NULL, 1, &taskHandles.mySerialTaskHandle);
    // BLE任务
    xTaskCreate(BLESensorTask, "BLESensor", 4096, NULL, 1, &taskHandles.bleTaskHandle);
    // Modbus任务
    xTaskCreate(ModbusServerTask, "ModbusServer", 4096, NULL, 1, &taskHandles.modbusTaskHandle);
    // HTTP任务
    xTaskCreate(HttpServerTask, "HttpServer", 8192, NULL, 1, &taskHandles.httpServerTaskHandle);
    // MQTT任务
    if(loadMQTTConfig(mqttConfig.ip, &mqttConfig.port, mqttConfig.username, mqttConfig.password)) {
      xTaskCreate(MQTTSubClientTask, "MQTTSubClient", 4096, NULL, 1, &taskHandles.mqttTaskHandle);
    }
    // 命令任务
    xTaskCreate(CommandTask, "Command", 4096, NULL, 1, &taskHandles.commandTaskHandle);
    // 网络任务
    xTaskCreate(NetworkTask, "Network", 2048, NULL, 1, &taskHandles.networkTaskHandle);
    // 其他任务
    xTaskCreate(MiscTask, "Misc", 2048, NULL, 1, &taskHandles.miscTaskHandle);
  }
}

void loopEntry() {
  taskYIELD();
}
