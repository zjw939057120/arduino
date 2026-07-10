#ifndef MQTT_SUB_CLIENT_H
#define MQTT_SUB_CLIENT_H

#include <Arduino.h>
//[MQTT@2.5.3]
#include <WiFi.h>
#include <MQTT.h>
#define MQTT_PORT 1883

extern MQTTClient client;

void MQTTSubClientStart();

void MQTTSubClientHandler();

#endif // MQTT_PUB_SUB_CLIENT_H
