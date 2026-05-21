#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ESP32Servo.h>

const char* WIFI_SSID = "Khu H";
const char* WIFI_PASS = "khuh1234";

String SECRET_KEY = "SUNTRAC123";

Servo horizontal;
Servo vertical;

int servohori = 90;
int servovert = 45;

int servohoriLimitHigh = 175;
int servohoriLimitLow = 5;
int servovertLimitHigh = 100;
int servovertLimitLow = 1;

#define LDR_TOP_LEFT 33
#define LDR_BOTTOM_LEFT 32
#define LDR_BOTTOM_RIGHT 35
#define LDR_TOP_RIGHT 34

#define SERVO_HORIZONTAL_PIN 18
#define SERVO_VERTICAL_PIN 19

int tolerance = 120;
int stepSize = 3;

unsigned long lastTracking = 0;
unsigned long lastLogTime = 0;
unsigned long lastWiFiCheck = 0;
bool wifiConnectedLogged = false;

enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL
};

ControlMode currentMode = MODE_AUTO;

WiFiUDP udp;
Coap coap(udp);

String modeText()
{
  return currentMode == MODE_AUTO ? "AUTO" : "MANUAL";
}

int readLDR(int pin)
{
  long total = 0;

  for (int i = 0; i < 10; i++)
  {
    total += analogRead(pin);
    delayMicroseconds(200);
  }

  int value = total / 10;
  return map(value, 0, 4095, 0, 1000);
}

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println();
  Serial.println("[WIFI] Connecting...");
}

void waitWiFi()
{
  connectWiFi();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  wifiConnectedLogged = true;

  Serial.println();
  Serial.println("[WIFI] Connected");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
}

void wifiTask()
{
  if (millis() - lastWiFiCheck < 5000)
    return;

  lastWiFiCheck = millis();

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!wifiConnectedLogged)
    {
      wifiConnectedLogged = true;
      Serial.println();
      Serial.println("[WIFI] Reconnected");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
    }

    return;
  }

  wifiConnectedLogged = false;
  Serial.println("[WIFI] Reconnecting...");

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

String getPayload(CoapPacket &packet)
{
  String payload = "";

  for (int i = 0; i < packet.payloadlen; i++)
  {
    payload += (char)packet.payload[i];
  }

  payload.trim();
  return payload;
}

void logCoapRx(const char* route, CoapPacket &packet, IPAddress ip, int port, const String &payload)
{
  Serial.print("[COAP RX] ");
  Serial.print(ip);
  Serial.print(":");
  Serial.print(port);
  Serial.print(" -> ");
  Serial.print(route);
  Serial.print(" msgid=");
  Serial.print(packet.messageid);
  Serial.print(" payload=");
  Serial.println(payload);
}

void sendCoapText(const char* route, IPAddress ip, int port, int messageid, const char* response)
{
  coap.sendResponse(ip, port, messageid, response);

  Serial.print("[COAP TX] ");
  Serial.print(route);
  Serial.print(" -> ");
  Serial.print(ip);
  Serial.print(":");
  Serial.print(port);
  Serial.print(" msgid=");
  Serial.print(messageid);
  Serial.print(" response=");
  Serial.println(response);
}

bool verifySecret(CoapPacket &packet, String &data)
{
  String payload = getPayload(packet);
  int index = payload.indexOf(':');

  if (index < 0)
  {
    data = "";
    return false;
  }

  String secret = payload.substring(0, index);
  data = payload.substring(index + 1);
  data.trim();

  return secret == SECRET_KEY;
}

String buildStateJson()
{
  String json = "{";

  json += "\"lt\":" + String(readLDR(LDR_TOP_LEFT)) + ",";
  json += "\"rt\":" + String(readLDR(LDR_TOP_RIGHT)) + ",";
  json += "\"ld\":" + String(readLDR(LDR_BOTTOM_LEFT)) + ",";
  json += "\"rd\":" + String(readLDR(LDR_BOTTOM_RIGHT)) + ",";

  json += "\"v\":" + String(servovert) + ",";
  json += "\"h\":" + String(servohori) + ",";

  json += "\"m\":\"";

  if(currentMode == MODE_AUTO)
  {
    json += "A";
  }
  else
  {
    json += "M";
  }

  json += "\"}";

  return json;
}

void logSystem()
{
  if (millis() - lastLogTime < 1000)
    return;

  lastLogTime = millis();

  Serial.println();
  Serial.println("========== SYSTEM ==========");
  Serial.print("WiFi: ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("CONNECTED | IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("DISCONNECTED");
  }

  Serial.print("Mode: ");
  Serial.println(modeText());

  Serial.print("LT: ");
  Serial.print(readLDR(LDR_TOP_LEFT));
  Serial.print(" | RT: ");
  Serial.print(readLDR(LDR_TOP_RIGHT));
  Serial.print(" | LD: ");
  Serial.print(readLDR(LDR_BOTTOM_LEFT));
  Serial.print(" | RD: ");
  Serial.println(readLDR(LDR_BOTTOM_RIGHT));

  Serial.print("Vertical Servo: ");
  Serial.println(servovert);
  Serial.print("Horizontal Servo: ");
  Serial.println(servohori);
  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.println("============================");
}

void callbackState(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;
  logCoapRx("state", packet, ip, port, payload);

  if (!verifySecret(packet, data))
  {
    sendCoapText("state", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  String response = buildStateJson();
  sendCoapText("state", ip, port, packet.messageid, response.c_str());
}

void callbackMode(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;
  logCoapRx("mode", packet, ip, port, payload);

  if (!verifySecret(packet, data))
  {
    sendCoapText("mode", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  data.toUpperCase();

  if (data == "AUTO")
  {
    currentMode = MODE_AUTO;
    sendCoapText("mode", ip, port, packet.messageid, "OK_AUTO");
  }
  else if (data == "MANUAL")
  {
    currentMode = MODE_MANUAL;
    sendCoapText("mode", ip, port, packet.messageid, "OK_MANUAL");
  }
  else
  {
    sendCoapText("mode", ip, port, packet.messageid, "INVALID_MODE");
  }
}

void callbackServo1(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;
  logCoapRx("servo1", packet, ip, port, payload);

  if (!verifySecret(packet, data))
  {
    sendCoapText("servo1", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  if (currentMode != MODE_MANUAL)
  {
    sendCoapText("servo1", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
    return;
  }

  servovert = constrain(data.toInt(), servovertLimitLow, servovertLimitHigh);
  vertical.write(servovert);

  Serial.print("[SERVO] Vertical -> ");
  Serial.println(servovert);

  sendCoapText("servo1", ip, port, packet.messageid, "OK");
}

void callbackServo2(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;
  logCoapRx("servo2", packet, ip, port, payload);

  if (!verifySecret(packet, data))
  {
    sendCoapText("servo2", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  if (currentMode != MODE_MANUAL)
  {
    sendCoapText("servo2", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
    return;
  }

  servohori = constrain(data.toInt(), servohoriLimitLow, servohoriLimitHigh);
  horizontal.write(servohori);

  Serial.print("[SERVO] Horizontal -> ");
  Serial.println(servohori);

  sendCoapText("servo2", ip, port, packet.messageid, "OK");
}

void setup()
{
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  horizontal.attach(SERVO_HORIZONTAL_PIN);
  vertical.attach(SERVO_VERTICAL_PIN);
  horizontal.write(servohori);
  vertical.write(servovert);

  delay(1000);

  waitWiFi();

  coap.server(callbackState, "state");
  coap.server(callbackMode, "mode");
  coap.server(callbackServo1, "servo1");
  coap.server(callbackServo2, "servo2");
  coap.start();

  Serial.println("[COAP] Server started on port 5683");
  Serial.println("[APP] 2 Axis Solar Tracker started");
}

void loop()
{
  wifiTask();

  if (WiFi.status() == WL_CONNECTED)
  {
    coap.loop();
  }

  logSystem();

  if (currentMode == MODE_AUTO && millis() - lastTracking >= 30)
  {
    lastTracking = millis();

    int lt = readLDR(LDR_TOP_LEFT);
    int rt = readLDR(LDR_TOP_RIGHT);
    int ld = readLDR(LDR_BOTTOM_LEFT);
    int rd = readLDR(LDR_BOTTOM_RIGHT);

    int avt = (lt + rt) / 2;
    int avd = (ld + rd) / 2;
    int avl = (lt + ld) / 2;
    int avr = (rt + rd) / 2;

    int dvert = avt - avd;
    int dhoriz = avl - avr;

    if (abs(dvert) > tolerance)
    {
      servovert += avt > avd ? stepSize : -stepSize;
      servovert = constrain(servovert, servovertLimitLow, servovertLimitHigh);
      vertical.write(servovert);
    }

    if (abs(dhoriz) > tolerance)
    {
      servohori += avl > avr ? -stepSize : stepSize;
      servohori = constrain(servohori, servohoriLimitLow, servohoriLimitHigh);
      horizontal.write(servohori);
    }
  }

  delay(2);
}
