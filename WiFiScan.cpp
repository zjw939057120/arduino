#include "Config.h"
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
// 命令队列大小
#define COMMAND_QUEUE_SIZE 8


// AT指令
#define AT_CMD_AT "AT\r\n"
// 重启指令
#define AT_CMD_RESTART "AT+RST\r\n"
// 获取WiFi状态指令
#define AT_CMD_CWLWAP "AT+CWLAP\r\n"
// 连接WiFi指令
#define AT_CMD_CWJAP "AT+CWJAP="
// BLE扫描指令
#define AT_CMD_BLE_SCAN "AT+BLESCAN="
// 获取BLE列表指令
#define AT_CMD_BLE_LST "AT+BLE_LST="
// UART定义指令
#define AT_CMD_UART_DEF "AT+UART_DEF="
// 获取WiFi状态指令
#define AT_CMD_CWSTATE "AT+CWSTATE?\r\n"

// 任务句柄
TaskHandles taskHandles = {NULL};
// WiFi配置
WiFiConfig wifiConfig;
// 设备配置
DeviceConfig deviceConfig;
// 重连次数
uint8_t reconnectCount = 0;

typedef struct {
  char cmd[SERIAL_BUFFER_SIZE];
} CommandMessage;

char serialBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;
Preferences preferences;
BLEScan* pBLEScan;
static QueueHandle_t commandQueue = NULL;

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
  if (preferences.getInt("count", 0) != bleCount) {
    preferences.putInt("count", bleCount);
  }
  String tmp = "";
  for (int i = 0; i < bleCount; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mac_%d", i);
    tmp = preferences.getString(key, "");
    if (!tmp.equals(bleMacs[i])) {
    preferences.putString(key, bleMacs[i]);
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
      strncpy(bleMacs[i], value.c_str(), 17);
      bleMacs[i][17] = '\0';
    }
  }
  preferences.end();
  return true;
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
  sendBleSensorData();
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
    //启用自动重连
    reconnectCount = 0;
    WiFi.setAutoReconnect(true);
    MySerial.println("WIFI CONNECTED");
    saveWiFiConfig(wifiConfig.ssid, wifiConfig.pwd);
  }
  break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
  {
    // 累计重连次数
    if (reconnectCount > 10) {
      // 重连次数超过 10 次
      WiFi.setAutoReconnect(false);
    }
    else {
      reconnectCount++;
    }

    MySerial.println("WIFI DISCONNECTED");
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
      MySerial.println("WIFI GOT IP");
      break;
    default:
      break;
    }

  ATCWState();// 获取当前 Wi-Fi 状态
}

void saveUartConfig(int baud, int dataBits, int stopBits, int parity, int addr) {
  preferences.begin(NVS_UART_NAMESPACE, false);
  if (preferences.getInt("baud", 115200) != baud) {
    preferences.putInt("baud", baud);
  }
  if (preferences.getInt("data_bits", 8) != dataBits) {
    preferences.putInt("data_bits", dataBits);
  }
  if (preferences.getInt("stop_bits", 1) != stopBits) {
    preferences.putInt("stop_bits", stopBits);
  }
  if (preferences.getInt("parity", 0) != parity) {
    preferences.putInt("parity", parity);
  }
  if (preferences.getInt("addr", 0) != addr) {
    preferences.putInt("addr", addr);
  }
  preferences.end();
}

bool loadUartConfig(int* baud, int* dataBits, int* stopBits, int* parity, int* addr) {
  preferences.begin(NVS_UART_NAMESPACE, true);
  *baud = preferences.getInt("baud", 9600);
  *dataBits = preferences.getInt("data_bits", 8);
  *stopBits = preferences.getInt("stop_bits", 1);
  *parity = preferences.getInt("parity", 0);
  *addr = preferences.getInt("addr", 0);
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
  // 加载MODBUS服务器是否禁用
  deviceConfig.modbusDisabled = preferences.getBool("modbusDisabled", false);
  // 加载HTTP服务器是否禁用
  deviceConfig.httpDisabled = preferences.getBool("httpDisabled", false);
  // 加载MQTT服务器是否禁用
  deviceConfig.mqttDisabled = preferences.getBool("mqttDisabled", false);
  preferences.end();

  // 加载AP配置
  // 获取设备名称
  getHostname(deviceConfig.ap_ssid);
  // 设置默认密码
  strcpy(deviceConfig.ap_pwd, "12345678");
  // 获取设备地址
  getMacAddress(deviceConfig.mac);
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

void ScanWiFi() {
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
}

void DoBLEScan(int duration) {
  ble_scan_lock = true;
  pBLEScan->stop();
  delay(10);
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

      delay(interval);
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

  strncpy(wifiConfig.ssid, ssid, sizeof(wifiConfig.ssid) - 1);
  wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';
  strncpy(wifiConfig.pwd, pwd, sizeof(wifiConfig.pwd) - 1);
  wifiConfig.pwd[sizeof(wifiConfig.pwd) - 1] = '\0';

  //启用自动重连
  reconnectCount = 0;
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pwd);
  MySerial.println("OK");
}

void processCommand(char* cmd) {
  if (strcmp(cmd, AT_CMD_AT) == 0) {
    //测试
    MySerial.println("OK");
  } else if (strcmp(cmd, AT_CMD_RESTART) == 0) {
    MySerial.println("OK");
    // 重启ESP32
    delay(1000);
    ESP.restart();
  } else if (strcmp(cmd, AT_CMD_CWLWAP) == 0) {
    // 获取WiFi状态
    ATCWState();
    //获取WiFi列表
    ScanWiFi();
  } else if (strncmp(cmd, AT_CMD_CWJAP, strlen(AT_CMD_CWJAP)) == 0) {
    //连接WiFi
    char ssid[33];
    char pwd[65];
    
    if (parseWiFiCommand(cmd, ssid, pwd)) {
      DoWiFiConnect(ssid, pwd);
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, AT_CMD_BLE_SCAN, strlen(AT_CMD_BLE_SCAN)) == 0) {
    //扫描BLE
    int mode = 0;
    int duration = 0;
    
    if (parseBLECommand(cmd, &mode, &duration,&filter_type,filter_param)) {
      DoBLEScan(duration);
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, AT_CMD_BLE_LST, strlen(AT_CMD_BLE_LST)) == 0) {
    //保存BLE列表
    int count = 0;
    if (parseBleListCommand(cmd, &count)) {
      bleCount = count;
      saveBleListConfig();
      sendBleListReport(count);
      MySerial.println("OK");
    } else {
      MySerial.println("ERROR");
    }
  } else if (strncmp(cmd, AT_CMD_UART_DEF, strlen(AT_CMD_UART_DEF)) == 0) {
    //设置串口参数
    int baud = 115200;
    int dataBits = 8;
    int stopBits = 1;
    int parity = 0;
    int addr = 0;

    if (parseUartConfigCommand(cmd, &baud, &dataBits, &stopBits, &parity, &addr)) {
      saveUartConfig(baud, dataBits, stopBits, parity, addr);
      sendUartConfigReport(baud, dataBits, stopBits, parity, addr);
    } else {
      MySerial.println("ERROR");
    }
  } else if (strcmp(cmd, AT_CMD_CWSTATE) == 0) {
    // 获取WiFi状态
    ATCWState();
  } else {
    // 无效指令
    Serial.println("ERROR: " + String(strlen(cmd)) + "," + String(cmd));
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
      // 环形缓冲区
      if (localIndex < SERIAL_BUFFER_SIZE - 1) {
        localBuffer[localIndex++] = c;
      } else {
        localIndex = 0;
      }
      // 处理AT命令
      if(localIndex > 1 && localBuffer[localIndex - 2] == '\r' && localBuffer[localIndex - 1] == '\n') {
        localBuffer[localIndex] = '\0';
        strncpy(msg.cmd, localBuffer, SERIAL_BUFFER_SIZE);
        if (commandQueue != NULL) {
          if (xQueueSend(commandQueue, &msg, 0) != pdTRUE) {
            Serial.println("ERROR: queue full");
          }
        }
        // 清空缓冲区
        localIndex = 0;
      }
    }

    delay(10);
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
      // 环形缓冲区
      if (localIndex < SERIAL_BUFFER_SIZE - 1) {
        localBuffer[localIndex++] = c;
      } else {
        localIndex = 0;
      }
      // 处理AT命令
      if(localIndex > 1 && localBuffer[localIndex - 2] == '\r' && localBuffer[localIndex - 1] == '\n') {
        localBuffer[localIndex] = '\0';
        strncpy(msg.cmd, localBuffer, SERIAL_BUFFER_SIZE);
        if (commandQueue != NULL) {
          if (xQueueSend(commandQueue, &msg, 0) != pdTRUE) {
            Serial.println("ERROR: queue full");
          }
        }
        // 清空缓冲区
        localIndex = 0;
      }
    }

    delay(10);
  }
}

// FreeRTOS任务函数，用于处理传感器数据
void BLESensorTask(void* pvParameters) {
  while (true) {
    delay(3000); // 延迟3秒后开始处理传感器数据
    if (bleCount == 0) {
      continue; // 如果没有配置BLE传感器，则跳过本次循环
    }
    else if (ble_scan_lock) {
      continue; // 如果正在扫描BLE设备，则跳过本次循环
    }
    // 设置回调函数
    pBLEScan->setAdvertisedDeviceCallbacks(&bleSensorCallback);
    pBLEScan->start(5, false); // 开始扫描5秒
    // 等待扫描完成
    sendBleSensorData();
  }
}

// FreeRTOS任务函数，用于处理Modbus TCP连接
void ModbusServerTask(void *pvParameters) {
  ModbusServerStart();
  while (true) {
    ModbusServerHandler();
  }
}

// FreeRTOS任务函数，用于处理HTTP连接
void HttpServerTask(void *pvParameters) {
  HttpServerStart();
  while (true) {
    HttpServerHandler();
  }
}

// FreeRTOS任务函数，用于处理MQTT连接
void MQTTSubClientTask(void *pvParameters) {
  MQTTSubClientStart();
  while (true) {
    MQTTSubClientHandler();
  }
}

// FreeRTOS任务函数，用于从队列中接收命令并处理
void CommandTask(void* pvParameters) {
  CommandMessage msg;
  while (true) {
    if (xQueueReceive(commandQueue, &msg, portMAX_DELAY) == pdTRUE) {
      processCommand(msg.cmd);
    }
  }
}

// FreeRTOS任务函数，用于处理其他任务
void MiscTask(void* pvParameters) {
  while (true) {
    delay(10 * 60 * 1000); // 延迟10分钟后开始处理其他任务
    // 关闭AP模式
    WiFi.AP.end();
    // 删除任务
    vTaskDelete(NULL);
    taskHandles.miscTaskHandle = NULL;
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
    if (bleMacs[i][0] != '\0' && strcmp(bleMacs[i], addr) == 0) {
      return i;// 找到匹配的设备，返回索引
    }
  }
  return -1; // 未找到匹配的设备，返回-1
}

int sendBleSensorData() {
  MySerial.flush();
  // 初始化未检测到的传感器数据为0
  for (int i = bleCount; i < MAX_BLE_ADDRESSES; i++) {
    bleSensorData[i].temp = 0;
    bleSensorData[i].hum = 0;
  }

  uint8_t cwState = getATCWState();
  int8_t rssi = WiFi.RSSI();
  MySerial.printf("+SENSOR:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", bleSensorData[0].temp, bleSensorData[0].hum, bleSensorData[1].temp, bleSensorData[1].hum, bleSensorData[2].temp, bleSensorData[2].hum, bleSensorData[3].temp, bleSensorData[3].hum, bleSensorData[4].temp, bleSensorData[4].hum, bleSensorData[5].temp, bleSensorData[5].hum, bleSensorData[6].temp, bleSensorData[6].hum, bleSensorData[7].temp, bleSensorData[7].hum, bleSensorData[8].temp, bleSensorData[8].hum, bleSensorData[9].temp, bleSensorData[9].hum, cwState, rssi);
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

  // 初始化BLE设备
  BLEDevice::init(deviceConfig.ap_ssid);
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleScanCallback);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // 初始化WiFi 设备
  WiFi.hostname(deviceConfig.ap_ssid);
  // 启用自动重连
  reconnectCount = 0;
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.STA.begin();

  // 创建AP模式
  WiFi.AP.create(deviceConfig.ap_ssid, deviceConfig.ap_pwd);
  WiFi.AP.begin();

  // 加载WiFi配置
  loadWiFiConfig(wifiConfig.ssid, wifiConfig.pwd);

  // 加载UART配置
  int uartBaud = 9600;
  int uartDataBits = 8;
  int uartStopBits = 1;
  int uartParity = 0;
  int uartAddr = 0;
  if (loadUartConfig(&uartBaud, &uartDataBits, &uartStopBits, &uartParity, &uartAddr)) {
    sendUartConfigReport(uartBaud, uartDataBits, uartStopBits, uartParity, uartAddr);
  }

  // 加载BLE列表配置
  if (loadBleListConfig()) {
    sendBleListReport(bleCount);
  }
  
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
    if (!deviceConfig.modbusDisabled) {
      xTaskCreate(ModbusServerTask, "ModbusServer", 4096, NULL, 1, &taskHandles.modbusTaskHandle);
    }
    // HTTP任务
    if (!deviceConfig.httpDisabled) {
      xTaskCreate(HttpServerTask, "HttpServer", 4096, NULL, 1, &taskHandles.httpServerTaskHandle);
    }
    // MQTT任务
    if (!deviceConfig.mqttDisabled) {
      xTaskCreate(MQTTSubClientTask, "MQTTSubClient", 4096, NULL, 1, &taskHandles.mqttTaskHandle);
    }
    // 命令任务
    xTaskCreate(CommandTask, "Command", 8192, NULL, 1, &taskHandles.commandTaskHandle);
    // 其他任务
    xTaskCreate(MiscTask, "Misc", 4096, NULL, 1, &taskHandles.miscTaskHandle);
  }
}

void loopEntry() {
  taskYIELD();
}
