#include "MQTTSubClient.h"
#include "WiFiScan.h"

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

MQTTConfig mqttConfig;

void connect() {
  Serial.print("checking wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(5000);// 等待5秒，确保WiFi连接稳定
  }

  Serial.print("\n mqtt connecting");
  while (!client.connect(mqttConfig.clientId.c_str(), mqttConfig.user.c_str(), mqttConfig.password.c_str())) {
    Serial.print(".");
    delay(5000);// 等待5秒，确保MQTT连接稳定
  }

  Serial.println("\n mqtt connected!");

  client.subscribe(mqttConfig.subTopic.c_str());
  // client.unsubscribe(mqttConfig.subTopic.c_str());
}

void messageReceived(String &topic, String &payload) {
  Serial.println("messageReceived: topic: " + topic + ",payload: " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void MQTTSubClientStart() {
  // 配置MQTT客户端参数
  mqttConfig.host = "8.135.10.183";
  mqttConfig.port = 23287;
  mqttConfig.user = "username";
  mqttConfig.password = "password";
  mqttConfig.clientId = deviceConfig.ap_ssid;
  mqttConfig.subTopic = "subTopic";
  // WiFi连接
  WiFi.begin(wifiConfig.ssid, wifiConfig.pwd);
  delay(5000);// 等待5秒，确保WiFi连接稳定
  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported
  // by Arduino. You need to set the IP address directly.
  client.begin(mqttConfig.host.c_str(), mqttConfig.port, net);
  client.onMessage(messageReceived);
  connect();
}

void MQTTSubClientHandler() {
  client.loop();

  if (!client.connected()) {
    connect();
  }

  // publish a message roughly every five seconds.
  if (millis() - lastMillis > 5000) {
    lastMillis = millis();
    client.publish(mqttConfig.subTopic.c_str(), "world");
  }
}
