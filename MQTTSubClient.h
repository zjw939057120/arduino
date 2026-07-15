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
} MQTTConfig;

extern MQTTConfig mqttConfig;

extern MQTTClient mqttClient;

void MQTTSubClientStart();

void MQTTSubClientHandler();

// MQTT服务器配置
// #define MQTT_SERVER_IP "8.135.10.183"
// #define MQTT_SERVER_PORT 23287
// #define MQTT_SERVER_USER "username"
// #define MQTT_SERVER_PASSWORD "password"
#define MQTT_SERVER_TOPIC_SENSOR "sensor"
#endif // MQTT_SUB_CLIENT_H
