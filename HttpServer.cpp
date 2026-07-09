#include "HttpServer.h"
#include <ESPmDNS.h>
#include "WiFiScan.h"

WebServer httpServer;

const int led = 13;

static void handleRoot()
{
  digitalWrite(led, 1);
  char temp[400];
  int sec = millis() / 1000;
  int hr = sec / 3600;
  int min = (sec / 60) % 60;
  sec = sec % 60;

  snprintf(
      temp, 400,

      "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP32!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",

      hr, min, sec);
  httpServer.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

static void handleNotFound()
{
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for (int i = 0; i < httpServer.args(); i++)
  {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }

  httpServer.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

static void drawGraph()
{
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10)
  {
    int y2 = rand() % 130;
    snprintf(temp, sizeof(temp), "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  httpServer.send(200, "image/svg+xml", out);
}

void HttpServerStart() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  // 启动mDNS服务
  char mac[18];
  getMacStrAddress(mac);
  MDNS.begin(mac);
  // 启动HTTP服务器
  httpServer.on("/", handleRoot);
  httpServer.on("/test.svg", drawGraph);
  httpServer.on("/inline", []()
                { httpServer.send(200, "text/plain", "this works as well"); });
  httpServer.onNotFound(handleNotFound);
  httpServer.begin(HTTP_PORT);
  Serial.println("HTTP server started");
}

void HttpServerHandler() {
  httpServer.handleClient();
  vTaskDelay(2 / portTICK_PERIOD_MS); // allow the cpu to switch to other tasks
}
