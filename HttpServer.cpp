#include "HttpServer.h"
#include "WiFiScan.h"
#include "Device.h"
#include "data/index.html.h"

WebServer webServer;

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
  // 首页
  webServer.on("/home", HTTP_GET, handleRequestHome_GET);
  webServer.on("/home", HTTP_POST, handleRequestHome_POST);
  // 地址设置
  webServer.on("/address", HTTP_GET, handleRequestAddress_GET);
  webServer.on("/address", HTTP_POST, handleRequestAddress_POST);
  // 报警阀值
  webServer.on("/alarm", HTTP_GET, handleRequestAlarm_GET);
  webServer.on("/alarm", HTTP_POST, handleRequestAlarm_POST);
  // 无线设置
  webServer.on("/wifi", HTTP_GET, handleRequestWifi_GET);
  webServer.on("/wifi", HTTP_POST, handleRequestWifi_POST);
  // 蓝牙设置
  webServer.on("/ble", HTTP_GET, handleRequestBle_GET);
  webServer.on("/ble", HTTP_POST, handleRequestBle_POST);
  // 网络设置
  webServer.on("/network", HTTP_GET, handleRequestNetwork_GET);
  webServer.on("/network", HTTP_POST, handleRequestNetwork_POST);
  // 服务设置
  webServer.on("/service", HTTP_GET, handleRequestService_GET);
  webServer.on("/service", HTTP_POST, handleRequestService_POST);
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

// 首页
void handleRequestHome_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestHome_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

void handleRequestHome_GET_Handler(char *content, int size) {
  // wifi状态
  uint8_t cwState = getATCWState();
  snprintf(content, size, "{\"code\":0,\"status\":%d,\"ssid\":\"%s\",\"rssi\":%d,\"localIP\":\"%s\",\"gatewayIP\":\"%s\",\"subnetMask\":\"%s\",\"bssid\":\"%s\",\"dnsIP\":\"%s\"}",
           cwState,
           WiFi.SSID().c_str(),
           WiFi.RSSI(),
           WiFi.localIP().toString().c_str(),
           WiFi.gatewayIP().toString().c_str(),
           WiFi.subnetMask().toString().c_str(),
           deviceConfig.mac,
           WiFi.dnsIP().toString().c_str());
}
void handleRequestHome_POST() {
}

// 地址设置
void  handleRequestAddress_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestAddress_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestAddress_GET_Handler(char *content, int size) {
  // 加载UART配置
    int addr = 0;
    int baud = 0;
    int databits = 0;
    int parity = 0;
    int stopbits = 0;
    loadUartConfig(&baud, &databits, &stopbits, &parity, &addr);

    // 构建JSON响应
    snprintf(content, size, "{\"code\":0,\"addr\":%d,\"baud\":%d,\"databits\":%d,\"parity\":%d,\"stopbits\":%d}", addr, baud, databits, parity, stopbits);
}
void handleRequestAddress_POST() {
    // 提取各个表单字段的值
    int addr = webServer.arg("addr").toInt();
    int baud = webServer.arg("baud").toInt();
    int databits = webServer.arg("databits").toInt();
    int parity = webServer.arg("parity").toInt();
    int stopbits = webServer.arg("stopbits").toInt();

    if (addr < 0 || addr > 255) {
      // 通讯地址范围0-255
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"Invalid Address\"}"));
      return;
    } else if (baud < 0 || baud > 115200) {
      // 波特率范围80-5000000
      // 5000000=5Mbps
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"Invalid Baud Rate\"}"));
      return;
    } else if (databits < 5 || databits > 9) {
      // 数据位范围5-9
      // 5bit,6bit,7bit,8bit,9bit
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"Invalid Dataatabits\"}"));
      return;
    } else if (parity < 0 || parity > 1) {
      // 校验位范围0-1
      // 0=None,1=Odd,2=Even
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"Invalid Parity\"}"));
      return;
    } else if (stopbits < 0 || stopbits > 3) {
      // 停止位范围0-3
      // 1=1bit,2=1.5bit,3=2bit
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"Invalid Stopbits\"}"));
      return;
    }
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

//报警阀值
void handleRequestAlarm_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestAlarm_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestAlarm_GET_Handler(char *content, int size) {
  
}
void handleRequestAlarm_POST() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestAlarm_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

// 无线设置
void handleRequestWifi_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestWifi_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestWifi_GET_Handler(char *content, int size) {
  
}
void handleRequestWifi_POST() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestWifi_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

// 蓝牙设置
void handleRequestBle_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestBle_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestBle_GET_Handler(char *content, int size) {
  
}
void handleRequestBle_POST() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestBle_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

// 网络设置
void handleRequestNetwork_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestNetwork_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestNetwork_GET_Handler(char *content, int size) {
  
}
void handleRequestNetwork_POST() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestNetwork_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

// 服务设置
void handleRequestService_GET() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestService_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}
void handleRequestService_GET_Handler(char *content, int size) {
  
}
void handleRequestService_POST() {
    char response[HTTP_CONTENT_LENGTH];
    handleRequestService_GET_Handler(response, sizeof(response));
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, response);
}

