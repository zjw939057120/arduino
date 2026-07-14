#include "MQTTSubClient.h"
#include "WiFiScan.h"
#include "Sensor.h"

WiFiClient net;
MQTTClient client;
// 上次传感器发布时间
unsigned long lastSensorMillis = 0;
// 上次设备发布时间
unsigned long lastDevicePublishMillis = 0;
// MQTT配置
MQTTConfig mqttConfig;
// MQTT消息缓冲区
char payload_buffer[255];

void connect() {
  Serial.println("checking wifi status");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(5000);// 等待5秒，确保WiFi连接稳定
  }

  Serial.println("mqtt connecting");
  while (!client.connect(mqttConfig.clientId.c_str(), mqttConfig.user.c_str(), mqttConfig.password.c_str())) {
    Serial.print(".");
    delay(5000);// 等待5秒，确保MQTT连接稳定
  }

  Serial.println("mqtt connected");

  // 订阅主题
  client.subscribe(MQTT_SERVER_TOPIC_SENSOR);
}

void messageReceived(String &topic, String &payload) {
  // 打印收到的消息
  Serial.println("topic: " + topic + ",payload: " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void MQTTSubClientStart() {
  // 配置MQTT客户端参数
  mqttConfig.host = MQTT_SERVER_IP;
  mqttConfig.port = MQTT_SERVER_PORT;
  mqttConfig.user = MQTT_SERVER_USER;
  mqttConfig.password = MQTT_SERVER_PASSWORD;
  mqttConfig.clientId = deviceConfig.ap_ssid;
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

  auto now = millis();
  if (now - lastSensorMillis > 5000) {
    // 每5秒发布一次传感器数据
    lastSensorMillis = now;
    sprintf(payload_buffer, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            sensorData.CO2, sensorData.CH2O, sensorData.TVOC, sensorData.PM25, sensorData.PM100, sensorData.TEMP, sensorData.RH, sensorData.PM10, sensorData.TYPE,
            bleSensorData[0].temp, bleSensorData[0].hum, bleSensorData[1].temp, bleSensorData[1].hum, bleSensorData[2].temp, bleSensorData[2].hum, bleSensorData[3].temp, bleSensorData[3].hum, bleSensorData[4].temp, bleSensorData[4].hum,
            bleSensorData[5].temp, bleSensorData[5].hum, bleSensorData[6].temp, bleSensorData[6].hum, bleSensorData[7].temp, bleSensorData[7].hum, bleSensorData[8].temp, bleSensorData[8].hum, bleSensorData[9].temp, bleSensorData[9].hum);
    client.publish(MQTT_SERVER_TOPIC_SENSOR, payload_buffer, strlen(payload_buffer));
  }

}
