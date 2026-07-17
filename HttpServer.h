#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>

#define HTTP_PORT 80

#define HTTP_TYPE_JSON "application/json;charset=utf-8"
#define HTTP_TYPE_HTML "text/html;charset=utf-8"
#define HTTP_CONTENT_LENGTH 256
#define HTTP_CONTENT_LENGTH_WIFI 256 * 6

extern WebServer webServer;

void HttpServerStart();

void HttpServerHandler();

// 首页
void handleRequestHome_GET();
void handleRequestHome_GET_Handler(char *content, int size);
void handleRequestHome_POST();

// 地址设置
void handleRequestAddress_GET();
void handleRequestAddress_GET_Handler(char *content, int size);
void handleRequestAddress_POST();
//报警阀值
void handleRequestAlarm_GET();
void handleRequestAlarm_GET_Handler(char *content, int size);
void handleRequestAlarm_POST();
// 无线设置
void handleRequestWifi_GET();
void handleRequestWifi_GET_Handler(char *content, int size);
void handleRequestWifi_POST();
// 蓝牙设置
void handleRequestBle_GET();
void handleRequestBle_GET_Handler(char *content, int size);
void handleRequestBle_POST();
// 网络设置
void handleRequestNetwork_GET();
void handleRequestNetwork_GET_Handler(char *content, int size);
void handleRequestNetwork_POST();
// 服务设置
void handleRequestService_GET();
void handleRequestService_GET_Handler(char *content, int size);
void handleRequestService_POST();


#endif // HTTP_SERVER_H
