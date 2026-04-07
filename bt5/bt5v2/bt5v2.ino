#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define USE_WIFI 1

// WIFI
const char* ssid = "HOANG TAN";
const char* password = "0795617961";

// WS
WebSocketsClient webSocket;
const char* host = "192.168.1.100";
const int port = 3000;

// SENSOR
#define DHTPIN 15
#define DHTTYPE DHT22

// HARDWARE
#define RELAY_PIN 18
#define BUZZER_PIN 17

// LIMIT
#define TEMP_LIMIT 28
#define HUMI_LIMIT 85
#define TEMP_ALARM 32
#define HUMI_ALARM 95

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

// STATE
enum AppState {
  NORMAL,
  WARNING,
  ALARM,
  ERROR_STATE
};

AppState appState = NORMAL;

// DATA
float temp = 0;
float humi = 0;
bool errorSensor = false;
int errorSensorCount = 0;

// TIMER
unsigned long lastSensor = 0;
unsigned long lastLCD = 0;
unsigned long buzzer_time = 0;

#if USE_WIFI
unsigned long lastReconnectWiFi = 0;
unsigned long lastReconnectWS = 0;
unsigned long lastSend = 0;
unsigned long lastHeartbeat = 0;

bool wsConnected = false;
#endif

// ================= WIFI =================
#if USE_WIFI
void connectWiFi() {
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
}
#endif

// ================= WS =================
#if USE_WIFI
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
  }
}
#endif

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

// ================= STATE =================
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
        digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
      }
      break;

    case ERROR_STATE:
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      break;
  }
}

// ================= LCD =================
char line1[17];
char line2[17];

void displayLCD() {
  // format cố định → KHÔNG chớp
  snprintf(line1, sizeof(line1), "T:%5.1fC", temp);
  snprintf(line2, sizeof(line2), "H:%5.1f%%", humi);

  lcd.setCursor(0,0);
  lcd.print(line1);

  lcd.setCursor(0,1);

  switch(appState) {
    case NORMAL:
      lcd.print(line2);
      lcd.print(" NOR");
      break;
    case WARNING:
      lcd.print(line2);
      lcd.print(" WAR");
      break;
    case ALARM:
      lcd.print(line2);
      lcd.print(" ALM");
      break;
    case ERROR_STATE:
      lcd.print("SENSOR ERROR   ");
      break;
  }
}

#if USE_WIFI
// ================= SEND =================
void sendData() {
  if(!wsConnected) return;

  String data = "{";
  data += "\"temp\":" + String(temp) + ",";
  data += "\"humi\":" + String(humi);
  data += "}";

  webSocket.sendTXT(data);
}
#endif

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

#if USE_WIFI
  connectWiFi();

  Serial.print("Waiting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
#endif
}

// ================= LOOP =================
void loop() {

#if USE_WIFI
  webSocket.loop();

  // reconnect WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectWiFi > 5000) {
      lastReconnectWiFi = millis();
      connectWiFi();
    }
  }

  // reconnect WS
  if (!wsConnected) {
    if (millis() - lastReconnectWS > 5000) {
      lastReconnectWS = millis();
      webSocket.begin(host, port, "/");
      webSocket.onEvent(webSocketEvent);
    }
  }
#endif

  // ===== SENSOR =====
  if(millis() - lastSensor > 2000) {
    lastSensor = millis();
    readSensor();
    updateAppState();
  }

  // ===== CONTROL =====
  handleAppState();

  // ===== LCD (1s update) =====
  if(millis() - lastLCD > 1000) {
    lastLCD = millis();
    displayLCD();
  }

#if USE_WIFI
  // ===== SEND =====
  if(millis() - lastSend > 3000) {
    lastSend = millis();
    sendData();
  }

  if(millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    webSocket.sendTXT("ping");
  }
#endif
}