#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "HOANG TAN";
const char* password = "0795617961";

// ================= WS =================
WebSocketsClient webSocket;
const char* host = "192.168.1.100";
const int port = 3000;

// ================= SENSOR =================
#define DHTPIN 15
#define DHTTYPE DHT22

// ================= HARDWARE =================
#define RELAY_PIN 18
#define BUZZER_PIN 17

// ================= LIMIT =================
#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// ================= STATE MACHINE =================
enum SystemState {
  SYS_INIT,
  SYS_WIFI,
  SYS_WS,
  SYS_RUNNING,
  SYS_ERROR
};

enum AppState {
  NORMAL,
  WARNING,
  ALARM,
  ERROR_STATE
};

SystemState sysState = SYS_INIT;
AppState appState = NORMAL;

// ================= DATA =================
float temp, humi;
bool errorSensor = false;
int errorSensorCount = 0;

// ================= CONTROL =================
bool fanOn = false;
bool buzzerOn = false;
bool alarmOn = false;

// ================= TIMER =================
unsigned long lastSend = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastReconnect = 0;
unsigned long buzzer_time = 0;

// ================= WS =================
bool wsConnected = false;

// ================= QUEUE =================
struct Message {
  int id;
  String data;
};

#define MAX_QUEUE 10
Message queue[MAX_QUEUE];
int queueSize = 0;

int currentMsgId = 0;
bool waitingAck = false;
unsigned long sendTime = 0;

// ================= WIFI =================
void connectWiFi() {
  WiFi.begin(ssid, password);
}

// ================= WS EVENT =================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_CONNECTED:
      wsConnected = true;
      break;

    case WStype_DISCONNECTED:
      wsConnected = false;
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);

      if(msg.startsWith("{\"ack\":")) {
        waitingAck = false;
        removeQueue();
        return;
      }

      if(msg == "OFF") {
        alarmOn = false;
        digitalWrite(BUZZER_PIN, HIGH);
      }
      break;
    }
  }
}

// ================= SENSOR =================
void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    errorSensorCount++;
    if(errorSensorCount >= 5) errorSensor = true;
    return;
  }

  temp = t;
  humi = h;

  errorSensor = false;
  errorSensorCount = 0;
}

// ================= APP STATE =================
void updateAppState() {

  if(errorSensor) {
    appState = ERROR_STATE;
  }
  else if(temp > TEMP_ALARM || humi > HUMI_ALARM) {
    appState = ALARM;
  }
  else if(temp > TEMP_LIMIT || humi > HUMI_LIMIT) {
    appState = WARNING;
  }
  else {
    appState = NORMAL;
  }
}

// ================= HANDLE APP =================
void handleAppState() {

  switch(appState) {

    case NORMAL:
      fanOn = false;
      alarmOn = false;
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      break;

    case WARNING:
      fanOn = true;
      digitalWrite(RELAY_PIN, HIGH);
      break;

    case ALARM:
      alarmOn = true;

      if(millis() - buzzer_time > 500) {
        buzzer_time = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, !buzzerOn);
      }
      break;

    case ERROR_STATE:
      fanOn = true;
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      break;
  }
}

// ================= SYSTEM STATE =================
void updateSystemState() {

  switch(sysState) {

    case SYS_INIT:
      sysState = SYS_WIFI;
      break;

    case SYS_WIFI:
      if(WiFi.status() == WL_CONNECTED) {
        sysState = SYS_WS;
      }
      break;

    case SYS_WS:
      if(wsConnected) {
        sysState = SYS_RUNNING;
      }
      break;

    case SYS_RUNNING:
      if(WiFi.status() != WL_CONNECTED) {
        sysState = SYS_ERROR;
      }
      break;

    case SYS_ERROR:
      break;
  }
}

// ================= HANDLE SYSTEM =================
void handleSystemState() {

  switch(sysState) {

    case SYS_WIFI:
      connectWiFi();
      break;

    case SYS_WS:
      webSocket.begin(host, port, "/");
      webSocket.onEvent(webSocketEvent);
      break;

    case SYS_ERROR:
      if(millis() - lastReconnect > 5000) {
        lastReconnect = millis();
        connectWiFi();
      }
      break;

    default:
      break;
  }
}

// ================= QUEUE =================
void pushQueue(String data) {
  if(queueSize < MAX_QUEUE) {
    queue[queueSize].id = currentMsgId++;
    queue[queueSize].data = data;
    queueSize++;
  }
}

void removeQueue() {
  for(int i=1;i<queueSize;i++) {
    queue[i-1] = queue[i];
  }
  queueSize--;
}

// ================= SEND =================
void sendMessage(Message msg) {
  if(!wsConnected) return;

  String packet = "{";
  packet += "\"id\":" + String(msg.id) + ",";
  packet += "\"data\":" + msg.data;
  packet += "}";

  webSocket.sendTXT(packet);

  waitingAck = true;
  sendTime = millis();
}

void processQueue() {
  if(queueSize == 0) return;

  if(!waitingAck) {
    sendMessage(queue[0]);
  }
}

void checkTimeout() {
  if(waitingAck && millis() - sendTime > 3000) {
    sendMessage(queue[0]);
  }
}

// ================= HEARTBEAT =================
void heartbeat() {
  if(millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    webSocket.sendTXT("{\"topic\":\"ping\"}");
  }
}

// ================= LCD =================
void displayLCD() {
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("T:"); lcd.print(temp);
  lcd.print(" H:"); lcd.print(humi);

  lcd.setCursor(0,1);

  switch(appState) {
    case NORMAL: lcd.print("NORMAL"); break;
    case WARNING: lcd.print("WARNING"); break;
    case ALARM: lcd.print("ALARM"); break;
    case ERROR_STATE: lcd.print("ERROR"); break;
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  lcd.init();
  lcd.backlight();

  dht.begin();
}

// ================= LOOP =================
void loop() {

  webSocket.loop();

  // ===== STATE MACHINE =====
  updateSystemState();
  handleSystemState();

  readSensor();
  updateAppState();
  handleAppState();

  // ===== DISPLAY =====
  displayLCD();

  // ===== SEND DATA =====
  if(millis() - lastSend > 2000) {
    lastSend = millis();

    String data = "{";
    data += "\"temp\":" + String(temp) + ",";
    data += "\"humi\":" + String(humi) + ",";
    data += "\"state\":" + String(appState);
    data += "}";

    pushQueue(data);
  }

  processQueue();
  checkTimeout();
  heartbeat();
}