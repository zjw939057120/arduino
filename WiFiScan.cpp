#include "WiFiScan.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#define SERIAL_BUFFER_SIZE 128

char serialBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;

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

bool parseCWJAP(char* cmd, char* ssid, char* pwd) {
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

void ScanWiFi() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("OK");
  } else {
    for (int i = 0; i < n; ++i) {
      char macStr[18];
      formatMacAddress(WiFi.BSSID(i), macStr);
      int ecn = getEcnValue(WiFi.encryptionType(i));
      Serial.printf("+CWLAP:(%d,\"%s\",%d,\"%s\",%d,0,0,\"\",\"\",\"\",0)\r\n",
                    ecn,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    macStr,
                    WiFi.channel(i));
      delay(10);
    }
    Serial.println("OK");
  }
  WiFi.scanDelete();
}

bool attemptConnect(char* ssid, char* pwd, wifi_auth_mode_t minSecurity) {
  WiFi.disconnect(true);
  delay(500);
  
  WiFi.setMinSecurity(minSecurity);
  WiFi.begin(ssid, pwd);
  
  int timeout = 10000;
  int interval = 500;
  int elapsed = 0;
  wl_status_t status;
  
  while (elapsed < timeout) {
    status = WiFi.status();
    
    if (status == WL_CONNECTED) {
      Serial.print("WIFI CONNECTED\r\n");
      Serial.print("WIFI GOT IP\r\n");
      Serial.printf("+CWSTATE:2,\"%s\"\r\n", ssid);
      Serial.print("OK\r\n");
      return true;
    }
    
    if (status == WL_CONNECT_FAILED) {
      return false;
    }
    
    if (status == WL_NO_SSID_AVAIL) {
      Serial.print("+CWJAP:3\r\n");
      Serial.printf("+CWSTATE:0,\"%s\"\r\n", ssid);
      Serial.print("ERROR\r\n");
      return true;
    }
    
    delay(interval);
    elapsed += interval;
  }
  
  return false;
}

void ConnectWiFi(char* ssid, char* pwd) {
  if (strlen(ssid) == 0) {
    Serial.print("+CWJAP:4\r\n");
    Serial.print("+CWSTATE:0,\"\"\r\n");
    Serial.print("ERROR\r\n");
    return;
  }
  
  if (attemptConnect(ssid, pwd, WIFI_AUTH_WPA2_PSK)) {
    return;
  }
  
  if (attemptConnect(ssid, pwd, WIFI_AUTH_OPEN)) {
    return;
  }
  
  Serial.print("+CWJAP:1\r\n");
  Serial.printf("+CWSTATE:0,\"%s\"\r\n", ssid);
  Serial.print("ERROR\r\n");
}

void processCommand(char* cmd) {
  Serial.println(cmd);
  
  if (strcmp(cmd, "AT") == 0) {
    Serial.println("OK");
  } else if (strcmp(cmd, "AT+CWLAP") == 0) {
    ScanWiFi();
  } else if (strncmp(cmd, "AT+CWJAP=", 9) == 0) {
    char ssid[33];
    char pwd[65];
    
    if (parseCWJAP(cmd, ssid, pwd)) {
      ConnectWiFi(ssid, pwd);
    } else {
      Serial.println("ERROR");
    }
  } else {
    Serial.println("ERROR");
  }
}

void setupEntry() {
  Serial.begin(115200);
  WiFi.STA.begin();
  Serial.println("Ready");
}

void loopEntry() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\r') {
      continue;
    }
    
    if (c == '\n') {
      if (bufferIndex > 0) {
        serialBuffer[bufferIndex] = '\0';
        processCommand(serialBuffer);
        bufferIndex = 0;
      }
    } else {
      if (bufferIndex < SERIAL_BUFFER_SIZE - 1) {
        serialBuffer[bufferIndex++] = c;
      } else {
        bufferIndex = 0;
      }
    }
  }
}