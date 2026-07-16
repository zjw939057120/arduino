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

extern WebServer webServer;

void HttpServerStart();

void HttpServerHandler();

void handleRequestAddress_GET();
void handleRequestAddress_POST();

#endif // HTTP_SERVER_H
