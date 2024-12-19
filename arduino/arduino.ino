#include <DHT.h>
#include <SoftwareSerial.h>

// *** Module phát hiện người ***
#define DHTPIN 3
#define DHTTYPE DHT11
const int pirPin = 4;

DHT dht(DHTPIN, DHTTYPE);
int motionState = 0;
unsigned long motionDetectedTime = 0;
unsigned long motionLostTime = 0;
int openDelay = 3000; // Thời gian trễ trước khi mở cửa
int closeDelay = 3000; // Thời gian trễ trước khi đóng cửa

// *** Module điều khiển đóng/mở cửa ***
const int ls1 = 7; // Công tắc hành trình vị trí mở
const int ls2 = 8; // Công tắc hành trình vị trí đóng
const int i1 = 12; // Điều khiển động cơ chiều 1
const int i2 = 13; // Điều khiển động cơ chiều 2
int e1 = 50; // Tốc độ động cơ

bool isOpening = false; // Cờ trạng thái mở cửa
bool isClosing = false; // Cờ trạng thái đóng cửa
unsigned long startTime = 0; // Thời gian bắt đầu mở/đóng
unsigned long openCloseDuration = 0; // Thời gian thực hiện mở/đóng cửa

// *** Module kết nối với ESP8266 để gửi/nhận dữ liệu ***
SoftwareSerial espSerial(10, 11);
String command = ""; // Lệnh nhận từ ESP8266
bool autoMode = false; // Chế độ tự động (ban đầu là tắt)

// *** Các biến đo nhiệt độ, độ ẩm ***
float temperature = 0.0;
float humidity = 0.0;

void setup() {
  Serial.begin(115200);

  // Cài đặt cảm biến PIR và DHT
  pinMode(pirPin, INPUT);
  dht.begin();

  // Cài đặt động cơ và công tắc hành trình
  pinMode(ls1, INPUT_PULLUP);
  pinMode(ls2, INPUT_PULLUP);
  pinMode(i1, OUTPUT);
  pinMode(i2, OUTPUT);

  // Cài đặt ESP8266
  espSerial.begin(9600);
  Serial.println("System ready...");
}

void loop() {
  static bool isDoorOpen = false; // Trạng thái cửa (đóng/mở)

  // Đọc dữ liệu từ ESP8266
  if (espSerial.available()) {
    command = espSerial.readStringUntil('\n');
    handleUserCommand(command, isDoorOpen);
  }

  // Xử lý chế độ tự động (nếu bật)
  if (autoMode) {
    handleMotionSensor(isDoorOpen);
  }

  // Đọc nhiệt độ và độ ẩm
  temperature = readTemperature();
  humidity = readHumidity();

  // Kiểm tra trạng thái cửa
  checkDoorState(isDoorOpen);
  delay(200);
}

// *** Module điều khiển động cơ mở/đóng cửa ***
void forward() {
  digitalWrite(i1, LOW);
  digitalWrite(i2, HIGH);
  analogWrite(e1, 150);
}

void goback() {
  digitalWrite(i1, HIGH);
  digitalWrite(i2, LOW);
  analogWrite(e1, 150);
}

void stopMotor() {
  digitalWrite(i1, LOW);
  digitalWrite(i2, LOW);
  analogWrite(e1, 0);
}

void openDoor() {
  Serial.println("Opening the door...");
  forward();
  startTime = millis();
  isOpening = true;
}

void closeDoor() {
  Serial.println("Closing the door...");
  goback();
  startTime = millis();
  isClosing = true;
}

// *** Module phát hiện người ***
void handleMotionSensor(bool &isDoorOpen) {
  motionState = digitalRead(pirPin);
  unsigned long currentTime = millis();

  if (motionState == HIGH) { // Nếu phát hiện chuyển động
    if (!isDoorOpen && motionDetectedTime == 0) {
      motionDetectedTime = currentTime;
    }
    if (!isDoorOpen && currentTime - motionDetectedTime >= openDelay) {
      openDoor();
      isDoorOpen = true;
      motionDetectedTime = 0;
    }
    motionLostTime = 0;
  } else { // Không phát hiện chuyển động
    if (isDoorOpen && motionLostTime == 0) {
      motionLostTime = currentTime;
    }
    if (isDoorOpen && currentTime - motionLostTime >= closeDelay) {
      closeDoor();
      isDoorOpen = false;
      motionLostTime = 0;
    }
    motionDetectedTime = 0;
  }
}

float readTemperature() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("Error reading temperature!");
    return -1;
  }
  return temp;
}

float readHumidity() {
  float hum = dht.readHumidity();
  if (isnan(hum)) {
    Serial.println("Error reading humidity!");
    return -1;
  }
  return hum;
}

// *** Module kiểm tra trạng thái cửa ***
void checkDoorState(bool &isDoorOpen) {
  int rls1 = digitalRead(ls1);
  int rls2 = digitalRead(ls2);

  if (isOpening && rls1 == 0) { // Cửa mở hoàn toàn
    openCloseDuration = millis() - startTime;
    stopMotor();
    isOpening = false;
    String message = "open:" + String(openCloseDuration) + ":" + String(temperature) + ":" + String(humidity);
    sendMessage(message);
    isDoorOpen = true;
    Serial.println("Door fully opened.");
  }

  if (isClosing && rls2 == 0) { // Cửa đóng hoàn toàn
    openCloseDuration = millis() - startTime;
    stopMotor();
    isClosing = false;
    String message = "close:" + String(openCloseDuration) + ":" + String(temperature) + ":" + String(humidity);
    sendMessage(message);
    isDoorOpen = false;
    Serial.println("Door fully closed.");
  }
}

// *** Module kết nối với ESP8266 ***
void sendMessage(String message) {
  espSerial.println(message);
  Serial.println("Message sent: " + message);
}

// *** Module điều khiển cửa ***
void handleUserCommand(String receivedCommand, bool &isDoorOpen) {
  receivedCommand.trim();
  Serial.println("Command received: " + receivedCommand);

  if (receivedCommand.indexOf("\"command\":\"open\"") >= 0) {
    openDoor();
    isDoorOpen = true;
  } else if (receivedCommand.indexOf("\"command\":\"close\"") >= 0) {
    closeDoor();
    isDoorOpen = false;
  } else if (receivedCommand.indexOf("\"command\":\"auto\"") >= 0) {
    autoMode = true;
  } else if (receivedCommand.indexOf("\"command\":\"manual\"") >= 0) {
    autoMode = false;
  } else if (receivedCommand.indexOf("setting_door") >= 0) {
    int firstColon = receivedCommand.indexOf(':');
    int secondColon = receivedCommand.indexOf(':', firstColon + 1);
    int thirdColon = receivedCommand.indexOf(':', secondColon + 1);

    openDelay = receivedCommand.substring(firstColon + 1, secondColon).toInt();
    closeDelay = receivedCommand.substring(secondColon + 1, thirdColon).toInt();
    e1 = receivedCommand.substring(thirdColon + 1).toInt();

    Serial.println("New settings applied:");
    Serial.println("Open delay: " + String(openDelay));
    Serial.println("Close delay: " + String(closeDelay));
    Serial.println("Motor speed: " + String(e1));
  }
}
