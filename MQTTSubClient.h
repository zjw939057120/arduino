#ifndef MQTT_SUB_CLIENT_H
#define MQTT_SUB_CLIENT_H

#include <WiFi.h>
#include <MQTT.h>

#define MQTT_PORT 1883

extern MQTTClient client;

void MQTTSubClientStart();

void MQTTSubClientHandler();

#endif // MQTT_PUB_SUB_CLIENT_H
