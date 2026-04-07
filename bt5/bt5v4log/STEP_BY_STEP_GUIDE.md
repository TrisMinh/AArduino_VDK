# Hướng Dẫn Xây Dựng `bt5v4log` Từng Bước

Tài liệu này hướng dẫn đi từ chương trình đơn giản nhất đến chương trình hoàn chỉnh theo đúng hướng của dự án `bt5v4log`.

Mục tiêu:
- Đi từ đọc cảm biến DHT22
- Hiển thị LCD
- Điều khiển relay và buzzer theo ngưỡng
- Kết nối WiFi
- Gửi dữ liệu qua WebSocket
- Tổ chức lại bằng state machine

Mỗi chương bên dưới là một chương trình đầy đủ, có thể chạy độc lập.

## 1. Phần Cứng Và Thư Viện

Phần cứng đang dùng trong dự án:
- ESP32
- DHT22 ở chân `15`
- Relay quạt ở chân `18`
- Buzzer ở chân `19`
- LCD I2C `0x27`, SDA `21`, SCL `22`

Thư viện cần cài:
- `DHT sensor library`
- `LiquidCrystal_I2C`
- `WebSocketsClient` hoặc `arduinoWebSockets`

Các ngưỡng đang dùng:
- `TEMP_LIMIT = 28`
- `HUMI_LIMIT = 85`
- `TEMP_ALARM = 32`
- `HUMI_ALARM = 95`

## 2. Chương 1: Đọc DHT22 Và In Ra Serial

Mục tiêu:
- Xác nhận cảm biến hoạt động
- Đọc nhiệt độ và độ ẩm theo chu kỳ
- Biết cách xử lý lỗi đọc cảm biến

```cpp
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

unsigned long lastSensor = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  Serial.println("Step 1: DHT22 + Serial");
}

void loop() {
  if (millis() - lastSensor >= 2000) {
    lastSensor = millis();

    float temp = dht.readTemperature();
    float humi = dht.readHumidity();

    if (isnan(temp) || isnan(humi)) {
      delay(50);
      temp = dht.readTemperature();
      humi = dht.readHumidity();
    }

    if (isnan(temp) || isnan(humi)) {
      Serial.println("Read DHT failed");
      return;
    }

    Serial.print("Temp: ");
    Serial.print(temp, 1);
    Serial.print(" C, Humi: ");
    Serial.print(humi, 1);
    Serial.println(" %");
  }
}
```

Logic:
- Mỗi 2 giây đọc cảm biến một lần
- Nếu đọc lỗi, thử lại sau `50 ms`
- Nếu vẫn lỗi thì in thông báo và bỏ qua chu kỳ đó

Khi nào chuyển sang chương tiếp:
- Serial ra giá trị ổn định
- Không còn lỗi đọc kéo dài

## 3. Chương 2: Thêm LCD Để Xem Trực Tiếp

Mục tiêu:
- Hiển thị nhiệt độ và độ ẩm lên LCD 16x2
- Làm quen với `Wire` và địa chỉ I2C

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastSensor = 0;
float temp = 0;
float humi = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("Step 2 Ready");
}

void loop() {
  if (millis() - lastSensor >= 2000) {
    lastSensor = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      delay(50);
      t = dht.readTemperature();
      h = dht.readHumidity();
    }

    if (isnan(t) || isnan(h)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sensor error");
      Serial.println("Read DHT failed");
      return;
    }

    temp = t;
    humi = h;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(humi, 1);
    lcd.print("%");

    Serial.print("Temp: ");
    Serial.print(temp, 1);
    Serial.print(" C, Humi: ");
    Serial.print(humi, 1);
    Serial.println(" %");
  }
}
```

Logic:
- `Wire.begin(21, 22)` đúng với dự án hiện tại
- LCD chỉ là lớp hiển thị, không quyết định trạng thái
- Vẫn giữ chu kỳ đọc 2 giây để cảm biến ổn định

## 4. Chương 3: Điều Khiển Relay Và Buzzer Theo Ngưỡng

Mục tiêu:
- Tạo phản ứng cục bộ, chưa cần mạng
- Hiểu rõ `NORMAL`, `WARNING`, `ALARM`
- Thêm cơ chế xác nhận 5 giây để chống nhấp nháy

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22

#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUZZER_ON LOW
#define BUZZER_OFF HIGH

#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95
#define APP_STATE_CONFIRM_MS 5000

enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

float temp = 0;
float humi = 0;
bool errorSensor = false;
int errorSensorCount = 0;

AppState appState = NORMAL;
AppState pendingAppState = NORMAL;
unsigned long appStateSince = 0;
unsigned long lastSensor = 0;
unsigned long buzzerTime = 0;
bool buzzerOn = false;

const char* appStateName(AppState state) {
  switch (state) {
    case NORMAL: return "NORMAL";
    case WARNING: return "WARNING";
    case ALARM: return "ALARM";
    case ERROR_STATE: return "ERROR";
  }
  return "UNKNOWN";
}

AppState getRawAppState() {
  if (errorSensor) return ERROR_STATE;
  if (temp > TEMP_ALARM || humi > HUMI_ALARM) return ALARM;
  if (temp > TEMP_LIMIT || humi > HUMI_LIMIT) return WARNING;
  return NORMAL;
}

void updateAppState() {
  AppState rawState = getRawAppState();

  if (rawState == appState) {
    pendingAppState = appState;
    appStateSince = 0;
    return;
  }

  if (rawState != pendingAppState) {
    pendingAppState = rawState;
    appStateSince = millis();
    return;
  }

  if (appStateSince != 0 && millis() - appStateSince >= APP_STATE_CONFIRM_MS) {
    appState = pendingAppState;
    appStateSince = 0;
    Serial.print("State -> ");
    Serial.println(appStateName(appState));
  }
}

void handleOutput() {
  switch (appState) {
    case NORMAL:
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      buzzerOn = false;
      break;

    case WARNING:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      buzzerOn = false;
      break;

    case ALARM:
      digitalWrite(RELAY_PIN, HIGH);
      if (millis() - buzzerTime >= 500) {
        buzzerTime = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, buzzerOn ? BUZZER_ON : BUZZER_OFF);
      }
      break;

    case ERROR_STATE:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, BUZZER_ON);
      break;
  }
}

void displayLCD() {
  lcd.clear();
  if (appState == ERROR_STATE) {
    lcd.setCursor(0, 0);
    lcd.print("Sensor error");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(humi, 1);
  lcd.print("% ");
  lcd.print(appStateName(appState));
}

void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    delay(50);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  if (isnan(t) || isnan(h)) {
    errorSensorCount++;
    if (errorSensorCount >= 5) errorSensor = true;
    return;
  }

  temp = t;
  humi = h;
  errorSensor = false;
  errorSensorCount = 0;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
}

void loop() {
  if (millis() - lastSensor >= 2000) {
    lastSensor = millis();
    readSensor();
    updateAppState();
    displayLCD();
  }

  handleOutput();
}
```

Logic:
- `rawState` là trạng thái đo được ngay lập tức
- `appState` là trạng thái chính thức đã qua xác nhận
- Nếu cảm biến chỉ vượt ngưỡng chốc lát, state sẽ không đổi ngay
- Cách này tránh relay và buzzer bật tắt liên tục khi giá trị dao động sát ngưỡng

## 5. Chương 4: Kết Nối WiFi

Mục tiêu:
- Đưa thiết bị lên mạng
- Tạo cơ chế retry định kỳ
- Tách riêng phần kết nối khỏi phần xử lý cảm biến

```cpp
#include <WiFi.h>

const char* ssid = "Khu H";
const char* password = "khuh1234";

unsigned long lastReconnectWiFi = 0;

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    static bool printed = false;
    if (!printed) {
      printed = true;
      Serial.println("WiFi connected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  if (millis() - lastReconnectWiFi >= 5000) {
    lastReconnectWiFi = millis();
    connectWiFi();
  }
}
```

Logic:
- Không gọi `WiFi.begin()` liên tục trong mọi vòng lặp
- Retry mỗi 5 giây là đủ
- Đây chính là ý tưởng nền của `SYS_WIFI`

## 6. Chương 5: Thêm WebSocket Gửi Dữ Liệu Realtime

Mục tiêu:
- Sau khi có WiFi thì mở WebSocket
- Gửi dữ liệu nhiệt độ, độ ẩm, trạng thái quạt
- Chuẩn bị cho bước state machine đầy đủ

```cpp
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22
#define RELAY_PIN 18

const char* ssid = "Khu H";
const char* password = "khuh1234";
const char* host = "10.85.7.197";
const int port = 3000;

WebSocketsClient webSocket;
DHT dht(DHTPIN, DHTTYPE);

bool wsConnected = false;
unsigned long lastReconnectWiFi = 0;
unsigned long lastReconnectWS = 0;
unsigned long lastSensor = 0;
unsigned long lastSend = 0;

float temp = 0;
float humi = 0;

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("WebSocket connected");
      break;

    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("WebSocket disconnected");
      break;

    case WStype_TEXT:
      Serial.print("WS RX: ");
      Serial.println((char*)payload);
      break;
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
}

void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) return;
  temp = t;
  humi = h;
}

void sendRealtime() {
  if (!wsConnected) return;
  if (millis() - lastSend < 3000) return;

  lastSend = millis();

  String data = "{";
  data += "\"temp\":" + String(temp, 1) + ",";
  data += "\"humi\":" + String(humi, 1) + ",";
  data += "\"fan\":" + String(digitalRead(RELAY_PIN));
  data += "}";

  webSocket.sendTXT(data);
  Serial.println(data);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  dht.begin();
  connectWiFi();
}

void loop() {
  webSocket.loop();

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectWiFi >= 5000) {
      lastReconnectWiFi = millis();
      connectWiFi();
    }
    return;
  }

  if (!wsConnected && millis() - lastReconnectWS >= 5000) {
    lastReconnectWS = millis();
    webSocket.begin(host, port, "/");
    webSocket.onEvent(webSocketEvent);
  }

  if (millis() - lastSensor >= 2000) {
    lastSensor = millis();
    readSensor();
  }

  sendRealtime();
}
```

Logic:
- WiFi là điều kiện nền
- WebSocket chỉ bắt đầu khi WiFi đã lên
- Gửi realtime mỗi 3 giây để tránh spam

## 7. Chương 6: State Machine Hệ Thống

Mục tiêu:
- Tách rõ trạng thái kết nối và trạng thái nghiệp vụ
- Code dễ đọc hơn, dễ debug hơn, dễ mở rộng hơn

Trong dự án này có 2 state machine:

1. `SystemState`
- Quản lý `INIT`, `WAIT_WIFI`, `WAIT_WS`, `RUNNING`, `ERROR`

2. `AppState`
- Quản lý `NORMAL`, `WARNING`, `ALARM`, `ERROR_STATE`

Ý tưởng:
- `SystemState` trả lời câu hỏi: thiết bị có online được không
- `AppState` trả lời câu hỏi: môi trường hiện tại có an toàn không

Sơ đồ:

```text
SystemState:
INIT -> WIFI -> WS -> RUNNING
RUNNING -> ERROR khi mất WiFi

AppState:
NORMAL -> WARNING -> ALARM
NORMAL/WARNING/ALARM -> ERROR_STATE khi lỗi sensor
```

## 8. Chương 7: Bản Rút Gọn Của Chương Trình Hoàn Chỉnh

Chương này là cấu trúc gần nhất với `bt5v4log.ino`, nhưng được viết theo kiểu học từng khối chức năng.

```cpp
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Wire.h>

#define DHTPIN 15
#define DHTTYPE DHT22
#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUZZER_ON LOW
#define BUZZER_OFF HIGH

#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95
#define APP_STATE_CONFIRM_MS 5000

const char* ssid = "Khu H";
const char* password = "khuh1234";
const char* host = "10.85.7.197";
const int port = 3000;

enum SystemState { SYS_INIT, SYS_WIFI, SYS_WS, SYS_RUNNING, SYS_ERROR };
enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };

WebSocketsClient webSocket;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

SystemState sysState = SYS_INIT;
AppState appState = NORMAL;
AppState lastState = NORMAL;
AppState pendingAppState = NORMAL;

float temp = 0;
float humi = 0;
bool errorSensor = false;
int errorSensorCount = 0;
bool wsConnected = false;
bool buzzerOn = false;

unsigned long lastSensor = 0;
unsigned long lastLCD = 0;
unsigned long lastSend = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastReconnectWiFi = 0;
unsigned long lastReconnectWS = 0;
unsigned long appStateSince = 0;
unsigned long buzzerTime = 0;

const char* appStateName(AppState state) {
  switch (state) {
    case NORMAL: return "NORMAL";
    case WARNING: return "WARNING";
    case ALARM: return "ALARM";
    case ERROR_STATE: return "ERROR";
  }
  return "UNKNOWN";
}

const char* systemStateName(SystemState state) {
  switch (state) {
    case SYS_INIT: return "INIT";
    case SYS_WIFI: return "WAIT_WIFI";
    case SYS_WS: return "WAIT_WS";
    case SYS_RUNNING: return "RUNNING";
    case SYS_ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

void logLine(const char* tag, const String& message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms][");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void connectWiFi() {
  logLine("WIFI", "Connecting");
  WiFi.begin(ssid, password);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      logLine("WS", "Connected");
      break;
    case WStype_DISCONNECTED:
      wsConnected = false;
      logLine("WS", "Disconnected");
      break;
    case WStype_TEXT:
      logLine("WS RX", String((char*)payload));
      break;
  }
}

void updateSystemState() {
  switch (sysState) {
    case SYS_INIT:
      sysState = SYS_WIFI;
      break;
    case SYS_WIFI:
      if (WiFi.status() == WL_CONNECTED) sysState = SYS_WS;
      break;
    case SYS_WS:
      if (wsConnected) sysState = SYS_RUNNING;
      break;
    case SYS_RUNNING:
      if (WiFi.status() != WL_CONNECTED) sysState = SYS_ERROR;
      break;
    case SYS_ERROR:
      if (WiFi.status() == WL_CONNECTED) sysState = SYS_WS;
      break;
  }
}

void handleSystemState() {
  switch (sysState) {
    case SYS_WIFI:
      if (millis() - lastReconnectWiFi >= 5000) {
        lastReconnectWiFi = millis();
        connectWiFi();
      }
      break;

    case SYS_WS:
      if (!wsConnected && millis() - lastReconnectWS >= 5000) {
        lastReconnectWS = millis();
        webSocket.begin(host, port, "/");
        webSocket.onEvent(webSocketEvent);
      }
      break;

    case SYS_ERROR:
      if (millis() - lastReconnectWiFi >= 5000) {
        lastReconnectWiFi = millis();
        connectWiFi();
      }
      break;

    default:
      break;
  }
}

void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    delay(50);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  if (isnan(t) || isnan(h)) {
    errorSensorCount++;
    if (errorSensorCount >= 5) errorSensor = true;
    return;
  }

  temp = t;
  humi = h;
  errorSensor = false;
  errorSensorCount = 0;
}

AppState getRawAppState() {
  if (errorSensor) return ERROR_STATE;
  if (temp > TEMP_ALARM || humi > HUMI_ALARM) return ALARM;
  if (temp > TEMP_LIMIT || humi > HUMI_LIMIT) return WARNING;
  return NORMAL;
}

void updateAppState() {
  AppState rawState = getRawAppState();

  if (rawState == appState) {
    pendingAppState = appState;
    appStateSince = 0;
    return;
  }

  if (rawState != pendingAppState) {
    pendingAppState = rawState;
    appStateSince = millis();
    return;
  }

  if (appStateSince != 0 && millis() - appStateSince >= APP_STATE_CONFIRM_MS) {
    appState = pendingAppState;
  }
}

void handleAppState() {
  switch (appState) {
    case NORMAL:
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      break;

    case WARNING:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      break;

    case ALARM:
      digitalWrite(RELAY_PIN, HIGH);
      if (millis() - buzzerTime >= 500) {
        buzzerTime = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, buzzerOn ? BUZZER_ON : BUZZER_OFF);
      }
      break;

    case ERROR_STATE:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, BUZZER_ON);
      break;
  }
}

void displayLCD() {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C     ");

  lcd.setCursor(0, 1);
  if (appState == ERROR_STATE) {
    lcd.print("SENSOR ERROR   ");
    return;
  }

  lcd.print("H:");
  lcd.print(humi, 1);
  lcd.print("% ");
  lcd.print(appStateName(appState));
  lcd.print("   ");
}

void sendRealtime() {
  if (!wsConnected) return;
  if (millis() - lastSend < 3000) return;

  lastSend = millis();

  String data = "{";
  data += "\"temp\":" + String(temp, 1) + ",";
  data += "\"humi\":" + String(humi, 1) + ",";
  data += "\"fan\":" + String(digitalRead(RELAY_PIN)) + ",";
  data += "\"app_state\":\"" + String(appStateName(appState)) + "\",";
  data += "\"system_state\":\"" + String(systemStateName(sysState)) + "\"";
  data += "}";

  webSocket.sendTXT(data);
}

void heartbeat() {
  if (wsConnected && millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    webSocket.sendTXT("{\"topic\":\"ping\"}");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
  lcd.init();
  lcd.backlight();
  dht.begin();
  connectWiFi();
}

void loop() {
  webSocket.loop();

  updateSystemState();
  handleSystemState();

  if (millis() - lastSensor >= 2000) {
    lastSensor = millis();
    readSensor();
    updateAppState();
  }

  handleAppState();

  if (millis() - lastLCD >= 1000) {
    lastLCD = millis();
    displayLCD();
  }

  sendRealtime();
  heartbeat();

  if (appState != lastState) {
    logLine("APP", String(appStateName(lastState)) + " -> " + appStateName(appState));
    lastState = appState;
  }
}
```

## 9. Cách Học Và Debug Theo Đúng Trình Tự

Nên học và kiểm tra theo thứ tự này:

1. Sensor
- Đảm bảo đọc DHT ổn định

2. LCD
- Chắc chắn hiển thị đúng

3. Relay và buzzer
- Test từng trạng thái cục bộ

4. Delay xác nhận 5 giây
- Quan sát trạng thái có còn nhấp nháy không

5. WiFi
- Kiểm tra retry và IP

6. WebSocket
- Xem có gửi được JSON không

7. State machine
- Quan sát log chuyển trạng thái

## 10. Liên Hệ Với Code Hiện Tại

File chính hiện tại:
- [bt5v4log.ino](c:\Users\minht\OneDrive\Tài liệu\Arduino\bt5\bt5v4log\bt5v4log.ino)
- [bt5v4log_2.ino](c:\Users\minht\OneDrive\Tài liệu\Arduino\bt5\bt5v4log\bt5v4log_2\bt5v4log_2.ino)

Tài liệu phân tích state machine hiện có:
- [STATE_MACHINE.md](c:\Users\minht\OneDrive\Tài liệu\Arduino\bt5\bt5v4log\STATE_MACHINE.md)

Nếu muốn mở rộng tiếp, nên thêm:
- hysteresis riêng cho ngưỡng xuống
- queue gửi sự kiện có ACK
- tự phục hồi tốt hơn từ `SYS_ERROR`
- tách code thành nhiều file `.h/.cpp`
