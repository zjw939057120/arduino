#include "HttpServer.h"
#include "WiFiScan.h"
#include "Device.h"
#include "data/index.html.h"

WebServer webServer;

static const char responsePortal[] = R"===(
<!DOCTYPE html><html><head><title>ESP32 CaptivePortal</title></head><body>
<h1>Hello World!</h1><p>This is a captive portal example page. All unknown http requests will
be redirected here.</p></body></html>
)===";

// handle root path
void handleRoot() {
  webServer.send_P(200, HTTP_TYPE_HTML, INDEX_HTML);
}

// handle not found path
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";

  for (int i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }

  webServer.send(404, HTTP_TYPE_HTML, message);
}

void HttpServerStart() {
  delay(5000); // 等待5秒，确保WiFi连接稳定

  // 启动mDNS服务器
  MDNS.begin(deviceConfig.ap_ssid);
  // 启用CORS
  webServer.enableCORS();
  // 处理根路径
  webServer.on("/", handleRoot);
  // 处理门户路径
  webServer.on("/portal", []() {
    webServer.send(200, HTTP_TYPE_HTML, responsePortal);
  });

  webServer.on("/address", HTTP_GET, handleRequestAddress_GET);
  webServer.on("/address", HTTP_POST, handleRequestAddress_POST);
  // 处理未找到的路径
  webServer.onNotFound(handleNotFound);
  // 启动HTTP服务器
  webServer.begin(HTTP_PORT);
  Serial.println("HTTP server started");
}

void HttpServerHandler() {
  webServer.handleClient();
  delay(5); // give CPU some idle time
}

void  handleRequestAddress_GET() {
  // 加载UART配置
    int addr = 0;
    int baud = 0;
    int databits = 0;
    int parity = 0;
    int stopbits = 0;
    loadUartConfig(&baud, &databits, &stopbits, &parity, &addr);

    // 构建JSON响应
    char response[HTTP_CONTENT_LENGTH];
    snprintf(response, sizeof(response), "{\"code\":0,\"addr\":%d,\"baud\":%d,\"databits\":%d,\"parity\":%d,\"stopbits\":%d}", addr, baud, databits, parity, stopbits);
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

void handleRequestAddress_POST() {
    // 提取各个表单字段的值
    int addr = webServer.arg("addr").toInt();
    int baud = webServer.arg("baud").toInt();
    int databits = webServer.arg("databits").toInt();
    int parity = webServer.arg("parity").toInt();
    int stopbits = webServer.arg("stopbits").toInt();

    if (addr < 0 || addr > 255) {
      webServer.send(200, HTTP_TYPE_JSON, "{\"code\":1,\"msg\":\"Invalid Address\"}");
      return;
    } else if (baud < 0 || baud > 115200) {
      webServer.send(200, HTTP_TYPE_JSON, "{\"code\":1,\"msg\":\"Invalid Baud Rate\"}");
      return;
    } else if (databits < 5 || databits > 8) {
      webServer.send(200, HTTP_TYPE_JSON, "{\"code\":1,\"msg\":\"Invalid Dataatabits\"}");
      return;
    } else if (parity < 0 || parity > 1) {
      webServer.send(200, HTTP_TYPE_JSON, "{\"code\":1,\"msg\":\"Invalid Parity\"}");
      return;
    } else if (stopbits < 0 || stopbits > 1) {
      webServer.send(200, HTTP_TYPE_JSON, "{\"code\":1,\"msg\":\"Invalid Stopbits\"}");
      return;
    }

    Serial.println("通讯地址: " + String(addr));
    Serial.println("波特率: " + String(baud));
    Serial.println("数据位: " + String(databits));
    Serial.println("校验位: " + String(parity));
    Serial.println("停止位: " + String(stopbits));
    //保存UART配置
    saveUartConfig(baud, databits, stopbits, parity, addr);
    // 发送配置报告
    sendUartConfigReport(baud, databits, stopbits, parity, addr);
    // 构建JSON响应
    char response[HTTP_CONTENT_LENGTH];
    snprintf(response, sizeof(response), "{\"code\":0,\"addr\":%d,\"baud\":%d,\"databits\":%d,\"parity\":%d,\"stopbits\":%d}", addr, baud, databits, parity, stopbits);
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
