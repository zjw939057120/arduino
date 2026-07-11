#ifndef MQTT_SUB_CLIENT_H
#define MQTT_SUB_CLIENT_H

#include <Arduino.h>
//[MQTT@2.5.3]
#include <WiFi.h>
#include <MQTT.h>

typedef struct {
    String host;
    int port;
    String user;
    String password;
    String clientId;
    String subTopic;
} MQTTConfig;

extern MQTTConfig mqttConfig;

extern MQTTClient client;

void MQTTSubClientStart();

void MQTTSubClientHandler();

#endif // MQTT_PUB_SUB_CLIENT_H
