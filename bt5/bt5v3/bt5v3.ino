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

// ================= LIMIT =================
#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// ================= STATE =================
enum SystemState { SYS_INIT, SYS_WIFI, SYS_WS, SYS_RUNNING, SYS_ERROR };
enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };

SystemState sysState = SYS_INIT;
AppState appState = NORMAL;

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
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
}

// ================= WS EVENT =================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_CONNECTED:
      Serial.println("WS Connected");
      wsConnected = true;
      break;

    case WStype_DISCONNECTED:
      Serial.println("WS Disconnected");
      wsConnected = false;
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);

      Serial.print("RX: ");
      Serial.println(msg);

      // FIX ACK
      if(msg == "ACK" || msg.startsWith("{\"ack\":")) {
        waitingAck = false;
        removeQueue();
        Serial.println("ACK RECEIVED");
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
    delay(50);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  if (isnan(t) || isnan(h)) {
    errorSensorCount++;
    if(errorSensorCount >= 5) errorSensor = true;
    Serial.println("DHT ERROR");
    return;
  }

  temp = t;
  humi = h;
  errorSensor = false;
  errorSensorCount = 0;

  Serial.print("T: "); Serial.print(temp);
  Serial.print(" H: "); Serial.println(humi);
}

// ================= APP STATE =================
void updateAppState() {
  if(errorSensor) appState = ERROR_STATE;
  else if(temp > TEMP_ALARM || humi > HUMI_ALARM) appState = ALARM;
  else if(temp > TEMP_LIMIT || humi > HUMI_LIMIT) appState = WARNING;
  else appState = NORMAL;
}

// ================= CONTROL =================
void handleAppState() {

  switch(appState) {

    case NORMAL:
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      break;

    case WARNING:
      digitalWrite(RELAY_PIN, HIGH);
      break;

    case ALARM:
      if(millis() - buzzer_time > 500) {
        buzzer_time = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, !buzzerOn);
      }
      break;

    case ERROR_STATE:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      break;
  }
}

// ================= SYSTEM =================
void updateSystemState() {
  switch(sysState) {
    case SYS_INIT: sysState = SYS_WIFI; break;
    case SYS_WIFI:
      if(WiFi.status() == WL_CONNECTED) sysState = SYS_WS;
      break;
    case SYS_WS:
      if(wsConnected) sysState = SYS_RUNNING;
      break;
    case SYS_RUNNING:
      if(WiFi.status() != WL_CONNECTED) sysState = SYS_ERROR;
      break;
    case SYS_ERROR:
      break;
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
          webSocket.begin(host, port, "/");
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

// ================= QUEUE =================
void pushQueue(String data) {
  if(queueSize < MAX_QUEUE) {
    queue[queueSize].id = currentMsgId++;
    queue[queueSize].data = data;
    queueSize++;

    Serial.print("QUEUE: ");
    Serial.println(data);
  }
}

void removeQueue() {
  if(queueSize <= 0) return;

  for(int i=1;i<queueSize;i++) {
    queue[i-1] = queue[i];
  }

  queueSize--;

  if(queueSize < 0) queueSize = 0;
}

// ================= SEND =================
void sendMessage(Message msg) {
  if(!wsConnected) return;

  String packet = "{";
  packet += "\"id\":" + String(msg.id) + ",";
  packet += "\"data\":" + msg.data;
  packet += "}";

  webSocket.sendTXT(packet);

  Serial.print("TX: ");
  Serial.println(packet);

  waitingAck = true;
  sendTime = millis();
}

void processQueue() {
  if(queueSize <= 0) return;

  if(!waitingAck && queueSize > 0) {
    sendMessage(queue[0]);
  }
}

void checkTimeout() {
  if(queueSize <= 0) return;

  if(waitingAck && millis() - sendTime > 3000) {
    Serial.println("RESEND...");
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
  digitalWrite(BUZZER_PIN, HIGH);

  lcd.init();
  lcd.backlight();

  dht.begin();

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

  if(millis() - lastLCD > 1000) {
    lastLCD = millis();
    displayLCD();
  }

  if(millis() - lastSend > 3000) {
    lastSend = millis();

    if(queueSize == 0) {   // 🔥 FIX flood queue
      String data = "{";
      data += "\"temp\":" + String(temp) + ",";
      data += "\"humi\":" + String(humi);
      data += "}";

      pushQueue(data);
    }
  }

  processQueue();
  checkTimeout();
  heartbeat();
}