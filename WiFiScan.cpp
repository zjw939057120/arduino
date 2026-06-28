#include "WiFiScan.h"
#include <Arduino.h>
#include <WiFi.h>

#define SERIAL_BUFFER_SIZE 64

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

void ScanWiFi() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println();
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
    Serial.println();
    Serial.println("OK");
  }
  WiFi.scanDelete();
}

void processCommand(char* cmd) {
  Serial.println(cmd);
  Serial.println();
  if (strcmp(cmd, "AT") == 0) {
    Serial.println("OK");
  } else if (strcmp(cmd, "AT+CWLAP") == 0) {
    ScanWiFi();
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