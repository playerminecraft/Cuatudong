#include <ESP8266WiFi.h>

#include <WebSocketsClient.h>

#include <SoftwareSerial.h>

const char * ssid = "Duyen Duong"; // Thay bằng tên WiFi của bạn
const char * password = "123456789"; // Thay bằng mật khẩu WiFi
const char * ws_server = "192.168.2.6"; // IP máy chủ Node.js

SoftwareSerial mySerial(D2, D1);
WebSocketsClient webSocket;

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600);
  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Kết nối WebSocket
  webSocket.begin(ws_server, 8080, "/");

  // Thiết lập callback khi nhận dữ liệu từ WebSocket Server
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  // Luôn giữ kết nối WebSocket
  webSocket.loop();
  if (mySerial.available()) {
    String rawData = mySerial.readStringUntil('\n'); // Đọc đến ký tự newline
    rawData.trim(); // Xóa khoảng trắng hoặc ký tự không cần thiết
    //Gửi dữ liệu lên server
    if (rawData.length() > 0) {
       Serial.println("Received Data: " + rawData);
        // String Data = "ESP:"+ rawData;
        webSocket.sendTXT(rawData); // Gửi dữ liệu đến server WebSocket
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    Serial.println("WebSocket connected");
    webSocket.sendTXT("Hello from ESP8266");
    break;

  case WStype_TEXT:
    if (String((char * ) payload).indexOf("timestamp") == -1) {
      if(String((char * ) payload).indexOf("door_setting") == -1){
      Serial.printf("Received: %s\n", payload);
      // Thêm code xử lý dữ liệu nhận được từ server tại đây
      mySerial.println(String((char * ) payload));
      if (String((char * ) payload) == "{\"command\":\"open\"}") {
        Serial.println("Command received: OPEN");
      } else if (String((char * ) payload) == "{\"command\":\"close\"}") {
        Serial.println("Command received: CLOSE");
      }
      }
    }

    break;

  case WStype_DISCONNECTED:
    Serial.println("WebSocket disconnected");
    break;
  }
}