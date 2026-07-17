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
  snprintf(content, size, "{\"code\":0,\"status\":%d,\"ssid\":\"%s\",\"rssi\":%d,\"local_ip\":\"%s\",\"gateway_ip\":\"%s\",\"subnet_mask\":\"%s\",\"bssid\":\"%s\",\"dns_ip\":\"%s\"}",
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
    int dataBits = 0;
    int parity = 0;
    int stopBits = 0;
    loadUartConfig(&baud, &dataBits, &stopBits, &parity, &addr);

    // 构建JSON响应
    snprintf(content, size, "{\"code\":0,\"addr\":%d,\"baud\":%d,\"dataBits\":%d,\"parity\":%d,\"stopBits\":%d}", addr, baud, dataBits, parity, stopBits);
}
void handleRequestAddress_POST() {
    // 提取各个表单字段的值
    int addr = webServer.arg("addr").toInt();
    int baud = webServer.arg("baud").toInt();
    int dataBits = webServer.arg("dataBits").toInt();
    int parity = webServer.arg("parity").toInt();
    int stopBits = webServer.arg("stopBits").toInt();

    if (addr < 0 || addr > 255) {
      // 通讯地址范围0-255
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"通讯地址错误\"}"));
      return;
    } else if (baud < 0 || baud > 115200) {
      // 波特率范围80-5000000
      // 5000000=5Mbps
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"波特率错误\"}"));
      return;
    } else if (dataBits < 5 || dataBits > 9) {
      // 数据位范围5-9
      // 5bit,6bit,7bit,8bit,9bit
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"数据位错误\"}"));
      return;
    } else if (parity < 0 || parity > 2) {
      // 校验位范围0-2
      // 0=None,1=Odd,2=Even
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"校验位错误\"}"));
      return;
    } else if (stopBits < 0 || stopBits > 3) {
      // 停止位范围0-3
      // 1=1bit,2=1.5bit,3=2bit
      webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":1,\"msg\":\"停止位错误\"}"));
      return;
    }
    // 保存UART配置
    doSaveUartConfig(baud, dataBits, stopBits, parity, addr);
    
    // 发送JSON的响应
    webServer.send(200, HTTP_TYPE_JSON, F("{\"code\":0,\"msg\":\"配置成功\"}"));
    return;
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
  loadDeviceConfig();
  snprintf(content, size, "{\"code\":0,\"local_ip\":\"%s\",\"subnet_mask\":\"%s\",\"gateway_ip\":\"%s\",\"dns_ip\":\"%s\"}", deviceConfig.local_ip, deviceConfig.subnet_mask, deviceConfig.gateway_ip, deviceConfig.dns_ip);
}
void handleRequestNetwork_POST() {
  // 提取各个表单字段的值
  String local_ip = webServer.arg("local_ip");
  String subnet_mask = webServer.arg("subnet_mask");
  String gateway_ip = webServer.arg("gateway_ip");
  String dns_ip = webServer.arg("dns_ip");

  char response[HTTP_CONTENT_LENGTH];
  if (local_ip.isEmpty() && subnet_mask.isEmpty() && gateway_ip.isEmpty() && dns_ip.isEmpty()) {
    //静态IP配置
    snprintf(response, sizeof(response), "{\"code\":0,\"msg\":\"静态IP配置成功\"}");
  }else if (!local_ip.isEmpty() && !subnet_mask.isEmpty() && !gateway_ip.isEmpty() && !dns_ip.isEmpty()) {
    // 动态IP配置
    snprintf(response, sizeof(response), "{\"code\":0,\"msg\":\"配置成功\"}");
  }else {
    snprintf(response, sizeof(response), "{\"code\":1,\"msg\":\"缺少必填项\"}");
    return;
  }

  // 保存设备配置
  doSaveDeviceConfig(local_ip.c_str(), gateway_ip.c_str(), subnet_mask.c_str(), dns_ip.c_str());

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

