#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Wire.h>

// ================= WIFI =================
const char* ssid = "Khu H";
const char* password = "khuh1234";

// ================= WS =================
WebSocketsClient webSocket;
const char* host = "10.85.7.197";
const int port = 3000;

// ================= SENSOR =================
#define DHTPIN 15
#define DHTTYPE DHT22

// ================= HARDWARE =================
#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUZZER_ON LOW
#define BUZZER_OFF HIGH

// ================= LIMIT =================
#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95
#define APP_STATE_CONFIRM_MS 5000

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// ================= STATE =================
enum SystemState { SYS_INIT, SYS_WIFI, SYS_WS, SYS_RUNNING, SYS_ERROR };
enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };

SystemState sysState = SYS_INIT;
AppState appState = NORMAL;
AppState lastState = NORMAL;
AppState pendingAppState = NORMAL;

// ================= DATA =================
float temp = 0, humi = 0;
bool errorSensor = false;
int errorSensorCount = 0;

// ================= CONTROL =================
bool buzzerOn = false;
unsigned long buzzer_time = 0;

// ================= TIMER =================
unsigned long lastSensor = 0;
unsigned long lastLCD = 0;
unsigned long lastReconnectWiFi = 0;
unsigned long lastReconnectWS = 0;
unsigned long lastSend = 0;
unsigned long lastHeartbeat = 0;
unsigned long appStateSince = 0;

// ================= WS =================
bool wsConnected = false;

// ================= LOG =================
const char* appStateName(AppState state) {
  switch(state) {
    case NORMAL: return "NORMAL";
    case WARNING: return "WARNING";
    case ALARM: return "ALARM";
    case ERROR_STATE: return "ERROR";
  }

  return "UNKNOWN";
}

const char* systemStateName(SystemState state) {
  switch(state) {
    case SYS_INIT: return "INIT";
    case SYS_WIFI: return "WAIT_WIFI";
    case SYS_WS: return "WAIT_WS";
    case SYS_RUNNING: return "RUNNING";
    case SYS_ERROR: return "ERROR";
  }

  return "UNKNOWN";
}

void logLine(const char* tag, const String& message) {
  Serial.print('[');
  Serial.print(millis());
  Serial.print(" ms]");
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

// ================= WIFI =================
void connectWiFi() {
  logLine("WIFI", "Connecting to SSID: " + String(ssid));
  WiFi.begin(ssid, password);
}

// ================= SYSTEM STATE =================
void updateSystemState() {
  SystemState previousState = sysState;

  switch(sysState) {
    case SYS_INIT:
      sysState = SYS_WIFI;
      break;

    case SYS_WIFI:
      if(WiFi.status() == WL_CONNECTED)
        sysState = SYS_WS;
      break;

    case SYS_WS:
      if(wsConnected)
        sysState = SYS_RUNNING;
      break;

    case SYS_RUNNING:
      if(WiFi.status() != WL_CONNECTED)
        sysState = SYS_ERROR;
      break;

    case SYS_ERROR:
      break;
  }

  if(sysState != previousState) {
    logLine("SYSTEM", String(systemStateName(previousState)) + " -> " + systemStateName(sysState));
  }
}

// ================= HANDLE SYSTEM =================
void handleSystemState() {

  switch(sysState) {

    case SYS_WIFI:
      if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectWiFi > 5000) {
          lastReconnectWiFi = millis();
          connectWiFi();
        }
      }
      break;

    case SYS_WS:
      if (!wsConnected) {
        if (millis() - lastReconnectWS > 5000) {
          lastReconnectWS = millis();
          logLine("WS", "Opening socket to " + String(host) + ":" + String(port));
          webSocket.begin(host, port, "/esp");
          webSocket.onEvent(webSocketEvent);
        }
      }
      break;

    case SYS_ERROR:
      if (millis() - lastReconnectWiFi > 5000) {
        lastReconnectWiFi = millis();
        connectWiFi();
      }
      break;
  }
}

// ================= WS EVENT =================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

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
      break;
    }
  }
}

// ================= SENSOR =================
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
    if(errorSensorCount >= 5) errorSensor = true;
    logLine("SENSOR", "Read failed, retryCount=" + String(errorSensorCount));
    return;
  }

  temp = t;
  humi = h;
  errorSensor = false;
  errorSensorCount = 0;

  logLine("SENSOR", "temp=" + String(temp, 1) + "C, humi=" + String(humi, 1) + "%");
}

// ================= APP STATE =================
AppState getRawAppState() {
  if(errorSensor) return ERROR_STATE;
  if(temp > TEMP_ALARM || humi > HUMI_ALARM) return ALARM;
  if(temp > TEMP_LIMIT || humi > HUMI_LIMIT) return WARNING;
  return NORMAL;
}

void updateAppState() {
  AppState rawState = getRawAppState();

  if(rawState == appState) {
    pendingAppState = appState;
    appStateSince = 0;
    return;
  }

  if(rawState != pendingAppState) {
    pendingAppState = rawState;
    appStateSince = millis();
    logLine("STATE WAIT", String(appStateName(appState)) + " -> " + appStateName(rawState) + " pending");
    return;
  }

  if(appStateSince != 0 && millis() - appStateSince >= APP_STATE_CONFIRM_MS) {
    appState = pendingAppState;
    appStateSince = 0;
  }
}

// ================= CONTROL =================
void handleAppState() {

  switch(appState) {

    case NORMAL:
      digitalWrite(RELAY_PIN, LOW);
      buzzerOn = false;
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      break;

    case WARNING:
      digitalWrite(RELAY_PIN, HIGH);
      buzzerOn = false;
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      break;

    case ALARM:
      digitalWrite(RELAY_PIN, HIGH);
      if(millis() - buzzer_time > 500) {
        buzzer_time = millis();
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

// ================= STATUS SEND =================
void sendStatus(bool forceSend = false) {
  if(!wsConnected) return;
  if(!forceSend && millis() - lastSend <= 3000) return;

  lastSend = millis();

  String data = "{";
  data += "\"temp\":" + String(temp) + ",";
  data += "\"humi\":" + String(humi) + ",";
  data += "\"fan\":" + String(digitalRead(RELAY_PIN)) + ",";
  data += "\"alarm\":" + String(appState == ALARM) + ",";
  data += "\"error\":" + String(appState == ERROR_STATE) + ",";
  data += "\"app_state\":\"" + String(appStateName(appState)) + "\",";
  data += "\"system_state\":\"" + String(systemStateName(sysState)) + "\"";
  data += "}";

  webSocket.sendTXT(data);

  logLine(forceSend ? "STATE TX" : "STATUS TX", data);
}

// ================= HEARTBEAT =================
void heartbeat() {
  if(millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    webSocket.sendTXT("{\"topic\":\"ping\"}");
    logLine("PING", "Heartbeat sent");
  }
}

// ================= LCD =================
char line1[17];
char line2[17];

void displayLCD() {

  snprintf(line1, sizeof(line1), "T:%5.1fC", temp);
  snprintf(line2, sizeof(line2), "H:%5.1f%%", humi);

  lcd.setCursor(0,0);
  lcd.print(line1);

  lcd.setCursor(0,1);

  switch(appState) {
    case NORMAL: lcd.print(line2); lcd.print(" NOR"); break;
    case WARNING: lcd.print(line2); lcd.print(" WAR"); break;
    case ALARM: lcd.print(line2); lcd.print(" ALM"); break;
    case ERROR_STATE: lcd.print("SENSOR ERROR   "); break;
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  Wire.begin(21,22);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);

  lcd.init();
  lcd.backlight();

  dht.begin();

  logLine("BOOT", "System starting");
  connectWiFi();
}

// ================= LOOP =================
void loop() {

  webSocket.loop();

  updateSystemState();
  handleSystemState();

  if(millis() - lastSensor > 2000) {
    lastSensor = millis();
    readSensor();
    updateAppState();
  }

  handleAppState();

  if(appState != lastState) {
    logLine("STATE", String(appStateName(lastState)) + " -> " + appStateName(appState));

    if(appState == ALARM) {
      buzzerOn = true;
      buzzer_time = millis();
      digitalWrite(BUZZER_PIN, BUZZER_ON);
    }

    sendStatus(true);
    lastState = appState;
  }

  if(millis() - lastLCD > 1000) {
    lastLCD = millis();
    displayLCD();
  }

  sendStatus();
  heartbeat();
}
