#include "HttpServer.h"
#include "WiFiScan.h"
#include "Device.h"

WebServer httpServer;

static const char responsePortal[] = R"===(
<!DOCTYPE html><html><head><title>ESP32 CaptivePortal</title></head><body>
<h1>Hello World!</h1><p>This is a captive portal example page. All unknown http requests will
be redirected here.</p></body></html>
)===";

// index page handler
void handleRoot() {
  httpServer.send(200, "text/plain", "Hello from esp32!");
}

// this will redirect unknown http req's to our captive portal page
// based on this redirect various systems could detect that WiFi AP has a captive portal page
void handleNotFound() {
  httpServer.sendHeader("Location", "/portal");
  httpServer.send(302, "text/plain", "redirect to captive portal");
}

void HttpServerStart() {
  delay(5000); // 等待5秒，确保WiFi连接稳定

  // 启动mDNS服务器
  MDNS.begin(deviceConfig.ap_ssid);
  // 启动HTTP服务器
  // serve a simple root page
  httpServer.on("/", handleRoot);

  // serve portal page
  httpServer.on("/portal", []() {
    httpServer.send(200, "text/html", responsePortal);
  });

  // all unknown pages are redirected to captive portal
  httpServer.onNotFound(handleNotFound);
  httpServer.begin(HTTP_PORT);
  Serial.println("HTTP server started");
}

void HttpServerHandler() {
  httpServer.handleClient();
  delay(5); // give CPU some idle time
}
