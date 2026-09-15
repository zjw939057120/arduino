#ifndef MQTT_SUB_CLIENT_H
#define MQTT_SUB_CLIENT_H

#include <Arduino.h>
//[MQTT@2.5.3]
#include <WiFi.h>
#include <MQTT.h>

typedef struct __attribute__((packed)) // 结构体内存紧凑
{
    char ip[16];// MQTT服务器IP地址
    int port;// MQTT服务器端口
    char username[16];// MQTT用户名
    char password[16];// MQTT密码
    char clientId[32];// MQTT客户端ID
    char prefix[32];// MQTT主题前缀
} MQTTConfig;

extern MQTTConfig mqttConfig;

extern MQTTClient mqttClient;

void MQTTSubClientStart();

void MQTTSubClientHandler();

// 重新连接MQTT服务器
void MQTTSubClientReConnect();

#endif // MQTT_SUB_CLIENT_H
