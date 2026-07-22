#ifndef MQTT_SUB_CLIENT_H
#define MQTT_SUB_CLIENT_H

#include <Arduino.h>
//[MQTT@2.5.3]
#include <WiFi.h>
#include <MQTT.h>

typedef struct {
    char ip[16];
    int port;
    char username[16];
    char password[16];
    char clientId[32];
    char topic[32];
} MQTTConfig;

extern MQTTConfig mqttConfig;

extern MQTTClient mqttClient;

void MQTTSubClientStart();

void MQTTSubClientHandler();

// 重新连接MQTT服务器
void MQTTSubClientReConnect();

#endif // MQTT_SUB_CLIENT_H
