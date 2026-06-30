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

#define SERIAL_BUFFER_SIZE 128
#define NVS_NAMESPACE "wifi_config"
#define NVS_UART_NAMESPACE "uart_config"
#define COMMAND_QUEUE_SIZE 8

typedef struct {
  char cmd[SERIAL_BUFFER_SIZE];
} CommandMessage;

char serialBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;
Preferences preferences;
BLEScan* pBLEScan;
static QueueHandle_t commandQueue = NULL;
static char pendingSSID[33] = "";
static char pendingPWD[65] = "";

class MyBLECallback : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice device) {
    String addr = device.getAddress().toString();
    int rssi = device.getRSSI();

    uint8_t* advData = device.getPayload();
    size_t advLen = device.getPayloadLength();
    char advDataStr[512] = "";
    for (size_t i = 0; i < advLen && i < 255; i++) {
      sprintf(advDataStr + i * 2, "%02X", advData[i]);
    }

    char scanRspStr[1] = "";

    int addrType = (device.getAddressType() == BLE_ADDR_PUBLIC) ? 0 : 1;

    MySerial.printf("+BLESCAN:\"%s\",%d,%s,%s,%d\r\n",
                  addr.c_str(), rssi, advDataStr, scanRspStr, addrType);
  }
};

MyBLECallback bleCallback;

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

bool parseBLECommand(char* cmd, int* mode, int* duration) {
  char* paramStart = cmd + 11;
  char* comma = strchr(paramStart, ',');

  if (comma == NULL) {
    return false;
  }

  *comma = '\0';
  *mode = atoi(paramStart);
  *duration = atoi(comma + 1);

  return (*mode == 1 && *duration > 0 && *duration <= 60);
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
        // pendingSSID[0] = '\0';
        // pendingPWD[0] = '\0';
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
  //wifi状态
  String currentSsid = WiFi.SSID();
  if (currentSsid.length() == 0 && pendingSSID[0] != '\0') {
    currentSsid = String(pendingSSID);
  }
  // Map esp WiFi status to +CWSTATE codes (0-4) per protocol
  auto computeCWState = []() -> int {
    wl_status_t status = WiFi.status();
    if (status == WL_IDLE_STATUS) {
      return 0; // 尚未进行任何 Wi-Fi 连接
    }
    if (status == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      if (ip != "0.0.0.0") return 2; // 已获取到 IPv4 地址
      return 1; // 已连接上 AP，但尚未获取到 IPv4
    }
    if (status == WL_DISCONNECTED || status == WL_CONNECTION_LOST || status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
      return 4; // 处于断开状态
    }
    return 3; // 正在进行连接或重连
  };

  int cwState = computeCWState();
  MySerial.printf("+CWSTATE:%d,\"%s\"\r\n", cwState, currentSsid.c_str());
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
  if (!pBLEScan) {
    MySerial.println("ERROR");
    return;
  }

  pBLEScan->clearResults();
  pBLEScan->start(duration, false);

  MySerial.println("OK");
  MySerial.println("+BLESCANDONE");
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
    
    if (parseBLECommand(cmd, &mode, &duration)) {
      DoBLEScan(duration);
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
  } else {
    // 无效指令
    return;
  }
}

void SerialTask(void* pvParameters) {
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

void CommandTask(void* pvParameters) {
  int uartBaud = 115200;
  int uartDataBits = 8;
  int uartStopBits = 1;
  int uartParity = 0;
  int uartAddr = 0;

  if (loadUartConfig(&uartBaud, &uartDataBits, &uartStopBits, &uartParity, &uartAddr)) {
    sendUartConfigReport(uartBaud, uartDataBits, uartStopBits, uartParity, uartAddr);
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
  pBLEScan->setAdvertisedDeviceCallbacks(&bleCallback);
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
    xTaskCreate(SerialTask, "SerialTask", 4096, NULL, 1, NULL);
    xTaskCreate(MySerialTask, "MySerialTask", 4096, NULL, 1, NULL);
    xTaskCreate(CommandTask, "CommandTask", 8192, NULL, 1, NULL);
  }
}

void loopEntry() {
  taskYIELD();
}
