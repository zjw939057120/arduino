#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>
#include <NetworkClient.h>
#include <WebServer.h>

#define HTTP_PORT 80

extern WebServer httpServer;

void HttpServerStart();

void HttpServerHandler();

#endif // HTTP_SERVER_H
