#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Wire.h> 
const char* ssid = "A Cuong 007";// WIFI
const char* password = "01699408589";

WebSocketsClient webSocket;// WS
const char* host = "192.168.1.86";
const int port = 3000;
// SENSOR
#define DHTPIN 15
#define DHTTYPE DHT22

#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUZZER_ON LOW
#define BUZZER_OFF HIGH
#define BTN_FAN_PIN 25
#define BTN_BUZZER_PIN 26
#define BTN_MODE_PIN 27
#define BUTTON_DEBOUNCE_MS 150
// LIMIT
#define TEMP_LIMIT 30
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95
#define APP_STATE_CONFIRM_MS 5000

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// STATE
enum SystemState { SYS_INIT, SYS_WIFI, SYS_WS, SYS_RUNNING, SYS_WIFI_LOST, SYS_WS_LOST };
enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };
enum ControlMode { MODE_AUTO, MODE_MANUAL };
enum FanState { FAN_OFF, FAN_ON };
enum BuzzerState { BUZZER_IDLE, BUZZER_STEADY, BUZZER_BLINK };

SystemState sysState = SYS_INIT; // TRẠNG THÁI BAN ĐẦU
AppState appState = NORMAL;
AppState lastState = NORMAL;
AppState pendingAppState = NORMAL;

ControlMode controlMode = MODE_AUTO;
ControlMode lastControlMode = MODE_AUTO;

FanState autoFanState = FAN_OFF;
FanState manualFanState = FAN_OFF;
FanState fanState = FAN_OFF;

BuzzerState autoBuzzerState = BUZZER_IDLE;
BuzzerState buzzerState = BUZZER_IDLE;

float temp = 0, humi = 0;// biến data
bool errorSensor = false;
int errorSensorCount = 0;

bool buzzerOn = false;// CONTROL
unsigned long buzzer_time = 0;

unsigned long lastSensor = 0;// bộ đếm
unsigned long lastLCD = 0;
unsigned long lastReconnectWiFi = 0;
unsigned long lastReconnectWS = 0;
unsigned long lastSend = 0;
unsigned long appStateSince = 0;
//debounce
unsigned long lastFanButtonTime = 0;
unsigned long lastBuzzerButtonTime = 0;
unsigned long lastModeButtonTime = 0;
bool lastFanButtonLevel = HIGH;
bool lastBuzzerButtonLevel = HIGH;
bool lastModeButtonLevel = HIGH;
bool needStatusUpdate = false;//FORSE SEND DATA
bool manualBuzzerLatchedOff = false; // CỜ TẮT MANUAL BUZZER (CÒI K TỰ KÊU LẠI)

bool wsConnected = false;// WS

// LOG
const char* appStateName(AppState state) {
  switch(state) {
    case NORMAL: return "NORMAL";
    case WARNING: return "WARNING";
    case ALARM: return "ALARM";
    case ERROR_STATE: return "ERROR";
  }
  return "UNKNOWN";
}
const char* controlModeName(ControlMode mode) {
  switch(mode) {
    case MODE_AUTO: return "AUTO";
    case MODE_MANUAL: return "MANUAL";
  }
  return "UNKNOWN";
}
const char* fanStateName(FanState state) {
  switch(state) {
    case FAN_OFF: return "OFF";
    case FAN_ON: return "ON";
  }
  return "UNKNOWN";
}
const char* buzzerStateName(BuzzerState state) {
  switch(state) {
    case BUZZER_IDLE: return "OFF";
    case BUZZER_STEADY: return "ON";
    case BUZZER_BLINK: return "BLINK";
  }
  return "UNKNOWN";
}
const char* systemStateName(SystemState state) {
  switch(state) {
    case SYS_INIT: return "INIT";
    case SYS_WIFI: return "WAIT_WIFI";
    case SYS_WS: return "WAIT_WS";
    case SYS_RUNNING: return "RUNNING";
    case SYS_WIFI_LOST: return "WIFI_LOST";
    case SYS_WS_LOST: return "WS_LOST";
  }
  return "UNKNOWN";
}

void logLine(const char* tag, const String& message) {
  Serial.print('['); Serial.print(millis()); Serial.print(" ms]");
  Serial.print('['); Serial.print(tag); Serial.print("] ");
  Serial.println(message);
}

bool buttonPressed(int pin, unsigned long& lastTime, bool& lastLevel) { // HÀM KIỂM TRA NHẤN NÚT HỢP LỆ
  bool currentLevel = digitalRead(pin);
  bool pressed = false;
  if(lastLevel == HIGH && currentLevel == LOW && millis() - lastTime > BUTTON_DEBOUNCE_MS) {
    lastTime = millis();
    pressed = true;
  }
  lastLevel = currentLevel;
  return pressed;
}

void connectWiFi() {
  logLine("WIFI", "Connecting to SSID: " + String(ssid));
  WiFi.begin(ssid, password);
}

void connectWS() {
  logLine("WS", "Opening socket to " + String(host) + ":" + String(port));
  webSocket.begin(host, port, "/esp");
  webSocket.onEvent(webSocketEvent);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {// WS EVENT
  switch(type) {
    case WStype_CONNECTED:
      logLine("WS", "Connected to " + String(host) + ":" + String(port) + String("/esp"));
      wsConnected = true;
      break;
    case WStype_DISCONNECTED:
      logLine("WS", "Disconnected");
      wsConnected = false;
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      logLine("WS RX", msg);
      handleWsCommand(msg);
      break;
    }
  }
}

void updateSystemState() {// SYSTEM STATE
  SystemState previousState = sysState;
  switch(sysState) {
    case SYS_INIT:
      sysState = SYS_WIFI;
      break;
    case SYS_WIFI:
    case SYS_WIFI_LOST:
      if(WiFi.status() == WL_CONNECTED)
        sysState = SYS_WS;
      break;

    case SYS_WS:
    case SYS_WS_LOST:
      if(WiFi.status() != WL_CONNECTED)
        sysState = SYS_WIFI_LOST;
      else if(wsConnected)
        sysState = SYS_RUNNING;
      break;

    case SYS_RUNNING:
      if(WiFi.status() != WL_CONNECTED)
        sysState = SYS_WIFI_LOST;
      else if(!wsConnected)
        sysState = SYS_WS_LOST;
      break;
  }
  if(sysState != previousState) {
    logLine("SYSTEM", String(systemStateName(previousState)) + " -> " + systemStateName(sysState));
  }
}

// HANDLE SYSTEM
void handleSystemState() {
  switch(sysState) {
    case SYS_WIFI:
    case SYS_WIFI_LOST:
      if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectWiFi > 5000) {
        lastReconnectWiFi = millis();
        connectWiFi();
      }
      break;

    case SYS_WS:
    case SYS_WS_LOST:
      if (!wsConnected && millis() - lastReconnectWS > 5000) {
        lastReconnectWS = millis();
        connectWS();
      }
      break;
  }
}

// SENSOR
void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    t = dht.readTemperature();
    h = dht.readHumidity();
  }
  if (isnan(t) || isnan(h)) {
    if (errorSensorCount < 5) {
      errorSensorCount++;
      if (errorSensorCount == 5) {
        errorSensor = true;
        logLine("SENSOR", "Error reading DHT22!");
      }
    }
    return;
  }
  temp = t; humi = h;
  errorSensor = false; errorSensorCount = 0; //reset bộ đếm lỗi
  logLine("SENSOR", "temp=" + String(temp, 1) + "C, humi=" + String(humi, 1) + "%");
}

// APP STATE
AppState getRawAppState() { //lấy trạng thái hiện tại của app
  if(errorSensor) return ERROR_STATE;
  if(temp > TEMP_ALARM || humi > HUMI_ALARM) return ALARM;
  if(temp > TEMP_LIMIT || humi > HUMI_LIMIT) return WARNING;
  return NORMAL;
}

void updateAppState() {
  AppState rawState = getRawAppState(); // so sánh trạng thái hiện tại với trạng thái chờ
  if(rawState == appState) { // giống thì k đổi gì
    pendingAppState = appState;
    appStateSince = 0;
    return;
  }
  if(rawState != pendingAppState) { //khác thì kích hoạt pending
    pendingAppState = rawState;
    appStateSince = millis();
    logLine("STATE WAIT", String(appStateName(appState)) + " -> " + appStateName(rawState) + " pending");
    return;
  }
  if(appStateSince != 0 && millis() - appStateSince >= APP_STATE_CONFIRM_MS) { // đủ thời gian thì đổi
    appState = pendingAppState;
    appStateSince = 0;
  }
}

// CONTROL
void updateAutoOutputState() { // trung gian chỉ gán vào biến của auto
  switch(appState) {
    case NORMAL:
      autoFanState = FAN_OFF;
      autoBuzzerState = BUZZER_IDLE;
      break;
    case WARNING:
      autoFanState = FAN_ON;
      autoBuzzerState = BUZZER_IDLE;
      break;
    case ALARM:
      autoFanState = FAN_ON;
      autoBuzzerState = BUZZER_BLINK;
      break;
    case ERROR_STATE:
      autoFanState = FAN_ON;
      autoBuzzerState = BUZZER_STEADY;
      break;
  }
}

void handleControlMode() {
  if(controlMode != lastControlMode) {
    logLine("MODE", String(controlModeName(lastControlMode)) + " -> " + controlModeName(controlMode));
    if(controlMode == MODE_MANUAL) {
      manualFanState = fanState;
    }
    lastControlMode = controlMode;
  }
}

void resolveOutputState() { // gán giá trị từ auto/manual vào biến cuối cùng
  if(controlMode == MODE_MANUAL) {
    if(manualBuzzerLatchedOff) {
      buzzerState = BUZZER_IDLE;
    } else {
      buzzerState = autoBuzzerState;
      if(buzzerState == BUZZER_IDLE) {
        manualBuzzerLatchedOff = true;
      }
    }
  } else {
    buzzerState = autoBuzzerState;
  }

  if(controlMode == MODE_AUTO) {
    fanState = autoFanState;
  } else {
    fanState = manualFanState;
  }
}

void applyOutputState() { // áp dụng vào phần cứng
  if(buzzerState != BUZZER_BLINK) {
    buzzer_time = 0;
  }
  digitalWrite(RELAY_PIN, fanState == FAN_ON ? HIGH : LOW);
  switch(buzzerState) {
    case BUZZER_IDLE:
      buzzerOn = false;
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      break;

    case BUZZER_STEADY:
      buzzerOn = true;
      digitalWrite(BUZZER_PIN, BUZZER_ON);
      break;

    case BUZZER_BLINK:
      if(!buzzerOn && buzzer_time == 0) {
        buzzerOn = true;
        buzzer_time = millis();
        digitalWrite(BUZZER_PIN, BUZZER_ON);
      }

      if(millis() - buzzer_time > 500) {
        buzzer_time = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, buzzerOn ? BUZZER_ON : BUZZER_OFF);
      }
      break;
  }
}

void handleAppState() {
  updateAutoOutputState();
  handleControlMode();
  resolveOutputState();
  applyOutputState();
}

void switchToManual() {
  controlMode = MODE_MANUAL;
  manualFanState = fanState;
  manualBuzzerLatchedOff = (buzzerState == BUZZER_IDLE);
}

void switchToAuto() {
  controlMode = MODE_AUTO;
  manualBuzzerLatchedOff = false;
}

// MANUAL CONTROL
void setManualFan(bool on) {
  manualFanState = on ? FAN_ON : FAN_OFF;
}

void setManualBuzzer(bool on) {
  // chỉ tắt k bật
  if(!on) {
    manualBuzzerLatchedOff = true;
    buzzerState = BUZZER_IDLE;
  }
}
// 3 hàm xử lý nút nhấn
void toggleMode() {
  if(controlMode == MODE_AUTO) switchToManual();
  else switchToAuto();
}

void toggleManualFan() {
  if(controlMode != MODE_MANUAL) return;
  setManualFan(manualFanState != FAN_ON);
}

void muteManualBuzzer() {
  if(controlMode != MODE_MANUAL) return;
  setManualBuzzer(false);
}

void handleButtons() {
  if(buttonPressed(BTN_MODE_PIN, lastModeButtonTime, lastModeButtonLevel)) {
    toggleMode();
    needStatusUpdate = true;
  }

  if(buttonPressed(BTN_FAN_PIN, lastFanButtonTime, lastFanButtonLevel)) {
    toggleManualFan();
    needStatusUpdate = true;
  }

  if(buttonPressed(BTN_BUZZER_PIN, lastBuzzerButtonTime, lastBuzzerButtonLevel)) {
    muteManualBuzzer();
    needStatusUpdate = true;
  }
}
// xử lý ws
void handleWsCommand(String msg) {
  if(msg == "{\"mode\":\"MANUAL\"}" || msg == "{\"mode\":\"manual\"}") {
    switchToManual();
    needStatusUpdate = true;
    return;
  }

  if(msg == "{\"mode\":\"AUTO\"}" || msg == "{\"mode\":\"auto\"}") {
    switchToAuto();
    needStatusUpdate = true;
    return;
  }

  if(msg == "{\"fan\":\"ON\"}" || msg == "{\"fan\":\"on\"}") {
    if(controlMode == MODE_MANUAL) {
      setManualFan(true);
      needStatusUpdate = true;
    }
    return;
  }

  if(msg == "{\"fan\":\"OFF\"}" || msg == "{\"fan\":\"off\"}") {
    if(controlMode == MODE_MANUAL) {
      setManualFan(false);
      needStatusUpdate = true;
    }
    return;
  }

  if(msg == "{\"buzzer\":\"OFF\"}" || msg == "{\"buzzer\":\"off\"}" ||
     msg == "{\"buzzer\":\"MUTE\"}" || msg == "{\"buzzer\":\"mute\"}") {
    if(controlMode == MODE_MANUAL) {
      setManualBuzzer(false);
      needStatusUpdate = true;
    }
    return;
  }
}

// STATUS SEND
void sendStatus(bool forceSend = false) {
  if(!wsConnected) return;
  if(!forceSend && millis() - lastSend <= 3000) return;

  lastSend = millis();

  String data = "{";
  data += "\"temp\":" + String(temp) + ",";
  data += "\"humi\":" + String(humi) + ",";
  data += "\"mode\":\"" + String(controlModeName(controlMode)) + "\",";
  data += "\"fan\":\"" + String(fanState == FAN_ON ? "ON" : "OFF") + "\",";
  data += "\"buzzer_state\":\"" + String(buzzerState == BUZZER_IDLE ? "OFF" : "ON") + "\",";
  
  data += "\"app_state\":\"" + String(appStateName(appState)) + "\"";
  data += "}";

  webSocket.sendTXT(data);

  logLine(forceSend ? "STATE TX" : "STATUS TX", data);
}

// LCD
void displayLCD() {
  lcd.setCursor(0, 0);
  if(appState == ERROR_STATE) lcd.print("SENSOR ERROR    ");
  else lcd.print("T:" + String(temp, 1) + "C H:" + String(humi, 1) + "%  ");

  lcd.setCursor(0, 1);
  String f = (fanState == FAN_ON) ? "ON" : "OF";
  String b = (buzzerState == BUZZER_STEADY) ? "ON" : (buzzerState == BUZZER_BLINK ? "BL" : "OF");
  String m = (controlMode == MODE_AUTO) ? "AUTO" : "MAN ";
  lcd.print("F:" + f + " B:" + b + " M:" + m);
}

// SETUP
void setup() {
  Serial.begin(115200);

  Wire.begin(21,22);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_FAN_PIN, INPUT_PULLUP);
  pinMode(BTN_BUZZER_PIN, INPUT_PULLUP);
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);

  lcd.init();
  lcd.backlight();

  dht.begin();

  logLine("BOOT", "System starting");
  connectWiFi();
}

// LOOP
void loop() {

  webSocket.loop();

  updateSystemState();
  handleSystemState();

  if(millis() - lastSensor > 2000) {
    lastSensor = millis();
    readSensor();
    updateAppState();
  }

  handleButtons();
  handleAppState();

  if(needStatusUpdate) {
    sendStatus(true);
    needStatusUpdate = false;
  }

  if(appState != lastState) {
    logLine("STATE", String(appStateName(lastState)) + " -> " + appStateName(appState));

    if(lastState == ERROR_STATE && appState == NORMAL) { // khởi động lại lcd tránh bị lỗi chữ
      lcd.init();      
      lcd.backlight(); 
    }

    sendStatus(true);
    lastState = appState;
  }

  if(millis() - lastLCD > 1000) {
    lastLCD = millis();
    displayLCD();
  }

  sendStatus();
}
