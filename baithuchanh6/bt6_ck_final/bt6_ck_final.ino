#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ESP32Servo.h>

// WIFI

const char* WIFI_SSID = "HOANG TAN";
const char* WIFI_PASS = "0795617961";

String SECRET_KEY = "SUNTRAC123";

// SERVO

Servo horizontal;
Servo vertical;

int servohori = 90;
int servovert = 45;

int servohoriLimitHigh = 170;
int servohoriLimitLow = 10;

int servovertLimitHigh = 90;
int servovertLimitLow = 10;

// LDR

#define LDR_TOP_LEFT 33
#define LDR_BOTTOM_LEFT 32
#define LDR_BOTTOM_RIGHT 35
#define LDR_TOP_RIGHT 34

// SERVO PINS

#define SERVO_HORIZONTAL_PIN 18
#define SERVO_VERTICAL_PIN 19

// TRACKING

int tolerance = 250;
int stepSize = 2;
const int TRACKING_INTERVAL_MS = 80;

// RAIN SENSOR

#define RAIN_SENSOR_PIN 36
const int RAIN_THRESHOLD = 2000; //nguowngx mưa

bool rainDetected = false;

// STEPPER MOTOR

#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

#define LIMIT_SW 12
// đc bước
const int STEPS_90_DEG = 1000;

const int STEP_DELAY_MS = 2;
const int UMBRELLA_STEP_DELAY_US = 2000;
const int UMBRELLA_STEPS_PER_TASK = 1; // số bước mỗi lần chạy, để tránh block quá lâu
const int HOME_DEBOUNCE_MS = 20; // tránh nhiễu công tắc hành trình
const int HOME_SETTLE_MS = 300; // thời gian chờ sau khi đóng đến khi xác nhận đóng xong
const int MAX_HOME_STEPS = 10000;
const int UMBRELLA_OPEN_DIRECTION = -1; //chiều quay ngược chiều kim đồng hồ
const int UMBRELLA_CLOSE_DIRECTION = 1;

int currentStep = 0; // vị trí step hiện tại (0-7)
long positionSteps = 0;

bool umbrellaOpened = false;

enum UmbrellaMotionState {
  UMBRELLA_IDLE,
  UMBRELLA_OPENING,
  UMBRELLA_HOMING,
  UMBRELLA_SETTLE
};

UmbrellaMotionState umbrellaMotionState = UMBRELLA_IDLE;

int umbrellaDirection = 0;
int umbrellaTargetSteps = 0;
int umbrellaMovedSteps = 0;
int umbrellaHomeSteps = 0;

unsigned long homePressedSince = 0;
unsigned long umbrellaSettleStart = 0;
unsigned long lastUmbrellaStepMicros = 0;

// SYSTEM

unsigned long lastTracking = 0;
unsigned long lastLogTime = 0;
unsigned long lastWiFiCheck = 0;

bool wifiConnectedLogged = false;
bool wifiEverConnected = false;
bool coapStarted = false;

// STATE MACHINE

enum ControlMode {
  MODE_AUTO,
  MODE_MANUAL
};

enum AppState {
  STATE_WIFI_CONNECTING,
  STATE_WIFI_LOST,
  STATE_AUTO_TRACKING,
  STATE_MANUAL_CONTROL,
  STATE_RAIN_PROTECTION
};

ControlMode currentMode = MODE_AUTO;

AppState currentState = STATE_WIFI_CONNECTING;
AppState previousState = STATE_WIFI_CONNECTING;

// COAP

WiFiUDP udp;
Coap coap(udp);

// STEP SEQUENCE

const int stepSequence[8][4] = { // dùng half-step để quay mượt hơn
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

// FUNCTION DECLARE
//callback coap 
void callbackState(CoapPacket &packet, IPAddress ip, int port);
void callbackMode(CoapPacket &packet, IPAddress ip, int port);
void callbackServo1(CoapPacket &packet, IPAddress ip, int port);
void callbackServo2(CoapPacket &packet, IPAddress ip, int port);
void callbackUmbrella(CoapPacket &packet, IPAddress ip, int port);
int readLDR(int pin);
int readRainSensor();
bool isRainDetected();

// TEXT / LOG / JSON

String modeText()
{
  return currentMode == MODE_AUTO ? "AUTO" : "MANUAL";
}

String stateText(AppState state)
{
  switch(state)
  {
    case STATE_WIFI_CONNECTING:
      return "WIFI_CONNECTING";
    case STATE_WIFI_LOST:
      return "WIFI_LOST";
    case STATE_AUTO_TRACKING:
      return "AUTO_TRACKING";
    case STATE_MANUAL_CONTROL:
      return "MANUAL_CONTROL";
    case STATE_RAIN_PROTECTION:
      return "RAIN_PROTECTION";
  }

  return "UNKNOWN";
}

void logCoapRx(const char* route, CoapPacket &packet, IPAddress ip, int port, const String &payload)
{
  Serial.print("[COAP RX] ");
  Serial.print(route);
  Serial.print(" -> ");
  Serial.println(payload);
}

void sendCoapText(const char* route, IPAddress ip, int port, int messageid, const char* response)
{
  coap.sendResponse(ip, port, messageid, response);

  Serial.print("[COAP TX] ");
  Serial.print(route);
  Serial.print(" -> ");
  Serial.println(response);
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
  json += "\"rain\":" + String(rainDetected ? 1 : 0) + ",";
  json += "\"umbrella\":" + String(umbrellaOpened ? 1 : 0) + ",";
  json += "\"mode\":\"" + modeText() + "\",";
  json += "\"state\":\"" + stateText(currentState) + "\"";
  json += "}";

  return json;
}

void logSystem()
{
  if(millis() - lastLogTime < 1000)
    return;

  lastLogTime = millis();

  Serial.println();
  Serial.println("========== SYSTEM ==========");
  Serial.print("Mode: ");
  Serial.println(modeText());
  Serial.print("State: ");
  Serial.println(stateText(currentState));
  Serial.print("IP: ");
  if(WiFi.status() == WL_CONNECTED)
    Serial.println(WiFi.localIP());
  else
    Serial.println("not_connected");
  Serial.print("Rain: ");
  Serial.println(rainDetected);
  Serial.print("Sensors: LT=");
  Serial.print(readLDR(LDR_TOP_LEFT));
  Serial.print(" RT=");
  Serial.print(readLDR(LDR_TOP_RIGHT));
  Serial.print(" LD=");
  Serial.print(readLDR(LDR_BOTTOM_LEFT));
  Serial.print(" RD=");
  Serial.print(readLDR(LDR_BOTTOM_RIGHT));
  Serial.print(" RainRaw=");
  Serial.print(readRainSensor());
  Serial.print(" RainThreshold=");
  Serial.print(RAIN_THRESHOLD);
  Serial.print(" RainDetected=");
  Serial.println(rainDetected ? 1 : 0);
  Serial.print("Umbrella: ");
  Serial.println(umbrellaOpened);
  Serial.println("============================");
}

// SENSOR / PAYLOAD HELPERS
// đọc lấy trung bình
int readLDR(int pin)
{
  long total = 0;

  for(int i = 0; i < 10; i++)
  {
    total += analogRead(pin);
  }

  return total / 10;
}

int readRainSensor()
{
  long total = 0;

  for(int i = 0; i < 10; i++)
  {
    total += analogRead(RAIN_SENSOR_PIN);
  }

  return total / 10;
}

bool isRainDetected()
{
  return readRainSensor() < RAIN_THRESHOLD;
}

String getPayload(CoapPacket &packet)
{
  String payload = "";

  for(int i = 0; i < packet.payloadlen; i++)
  {
    payload += (char)packet.payload[i];
  }

  payload.trim();
  return payload;
}

bool verifySecret(CoapPacket &packet, String &data)
{
  String payload = getPayload(packet);
  int index = payload.indexOf(':');

  if(index < 0)
  {
    data = "";
    return false;
  }

  String secret = payload.substring(0, index);
  data = payload.substring(index + 1);
  data.trim();

  return secret == SECRET_KEY;
}

bool isHomePressed()
{
  if(digitalRead(LIMIT_SW) == LOW)
  {
    delay(20);
    return digitalRead(LIMIT_SW) == LOW;
  }

  return false;
}

// WIFI / COAP

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println();
  Serial.println("[WIFI] Connecting...");
}

void wifiTask()
{
  if(lastWiFiCheck != 0 && millis() - lastWiFiCheck < 5000)
    return;

  lastWiFiCheck = millis();

  if(WiFi.status() == WL_CONNECTED)
  {
    if(!wifiConnectedLogged) // tránh in serial liên tục
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

void startCoapServer()
{
  if(coapStarted)
    return;

  coap.server(callbackState, "state");
  coap.server(callbackMode, "mode");
  coap.server(callbackServo1, "servo1");
  coap.server(callbackServo2, "servo2");
  coap.server(callbackUmbrella, "umbrella");
  coap.start();

  coapStarted = true;

  Serial.println("[COAP] Started");
}

// STEPPER BASIC

void writeStep(int stepIndex)
{
  digitalWrite(IN1, stepSequence[stepIndex][0]);
  digitalWrite(IN2, stepSequence[stepIndex][1]);
  digitalWrite(IN3, stepSequence[stepIndex][2]);
  digitalWrite(IN4, stepSequence[stepIndex][3]);
}

void oneStep(int direction)
{
  currentStep += direction;

  if(currentStep > 7)
    currentStep = 0;

  if(currentStep < 0)
    currentStep = 7;

  writeStep(currentStep);
  delay(STEP_DELAY_MS);
}

void oneStepNoDelay(int direction)
{
  currentStep += direction;

  if(currentStep > 7)
    currentStep = 0;

  if(currentStep < 0)
    currentStep = 7;

  writeStep(currentStep);
}

void stepMotor(int steps, int direction)
{
  for(int i = 0; i < steps; i++)
  {
    oneStep(direction);
    positionSteps += direction;
  }
}

void motorOff()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// UMBRELLA MOTOR

bool homeMotor()
{
  Serial.println("[UMBRELLA] Homing");

  int count = 0;

  while(!isHomePressed() && count < MAX_HOME_STEPS)
  {
    oneStep(UMBRELLA_CLOSE_DIRECTION);
    count++;
  }

  if(!isHomePressed())
  {
    Serial.println("[UMBRELLA] Home ERROR");
    motorOff();
    return false;
  }

  positionSteps = 0;

  Serial.println("[UMBRELLA] Home OK");

  delay(300);
  return true;
}

void openUmbrella()
{
  if(umbrellaOpened && umbrellaMotionState == UMBRELLA_IDLE)
    return;

  if(umbrellaMotionState == UMBRELLA_OPENING)
    return;

  Serial.println("[UMBRELLA] OPEN");

  umbrellaDirection = UMBRELLA_OPEN_DIRECTION;
  umbrellaTargetSteps = constrain(STEPS_90_DEG - positionSteps, 0, STEPS_90_DEG);
  umbrellaMovedSteps = 0;
  lastUmbrellaStepMicros = 0;
  umbrellaMotionState = UMBRELLA_OPENING;
}

void startUmbrellaHome()
{
  if(umbrellaMotionState == UMBRELLA_HOMING || umbrellaMotionState == UMBRELLA_SETTLE)
    return;

  Serial.println("[UMBRELLA] CLOSE");

  umbrellaDirection = UMBRELLA_CLOSE_DIRECTION;
  umbrellaHomeSteps = 0;
  homePressedSince = 0;
  lastUmbrellaStepMicros = 0;
  umbrellaMotionState = UMBRELLA_HOMING;
}

void umbrellaMotionTask()
{
  if(umbrellaMotionState == UMBRELLA_IDLE)
    return;

  unsigned long now = millis();
  unsigned long nowMicros = micros();
  int stepsThisTask = 0;

  if(lastUmbrellaStepMicros == 0)
    lastUmbrellaStepMicros = nowMicros - UMBRELLA_STEP_DELAY_US;

  if(umbrellaMotionState == UMBRELLA_OPENING)
  {
    while(stepsThisTask < UMBRELLA_STEPS_PER_TASK &&
          umbrellaMovedSteps < umbrellaTargetSteps &&
          nowMicros - lastUmbrellaStepMicros >= UMBRELLA_STEP_DELAY_US)
    {
      lastUmbrellaStepMicros += UMBRELLA_STEP_DELAY_US;
      oneStepNoDelay(umbrellaDirection);

      positionSteps++;
      umbrellaMovedSteps++;
      stepsThisTask++;
    }

    if(umbrellaMovedSteps >= umbrellaTargetSteps)
    {
      umbrellaOpened = true;
      umbrellaMotionState = UMBRELLA_IDLE;
      lastUmbrellaStepMicros = 0;
      motorOff();
    }

    return;
  }

  if(umbrellaMotionState == UMBRELLA_HOMING)
  {
    if(digitalRead(LIMIT_SW) == LOW)
    {
      if(homePressedSince == 0)
      {
        homePressedSince = now;
        return;
      }

      if(now - homePressedSince >= HOME_DEBOUNCE_MS)
      {
        positionSteps = 0;
        umbrellaSettleStart = now;
        lastUmbrellaStepMicros = 0;
        umbrellaMotionState = UMBRELLA_SETTLE;
      }

      return;
    }

    homePressedSince = 0;

    if(umbrellaHomeSteps >= MAX_HOME_STEPS)
    {
      Serial.println("[UMBRELLA] Home ERROR");

      umbrellaMotionState = UMBRELLA_IDLE;
      motorOff();
      return;
    }

    while(stepsThisTask < UMBRELLA_STEPS_PER_TASK && umbrellaHomeSteps < MAX_HOME_STEPS)
    {
      if(digitalRead(LIMIT_SW) == LOW)
        return;

      if(nowMicros - lastUmbrellaStepMicros < UMBRELLA_STEP_DELAY_US)
        return;

      lastUmbrellaStepMicros += UMBRELLA_STEP_DELAY_US;
      oneStepNoDelay(umbrellaDirection);

      if(positionSteps > 0)
        positionSteps--;

      umbrellaHomeSteps++;
      stepsThisTask++;
    }

    return;
  }

  if(umbrellaMotionState == UMBRELLA_SETTLE)
  {
    if(now - umbrellaSettleStart < HOME_SETTLE_MS)
      return;

    Serial.println("[UMBRELLA] Home OK");

    umbrellaOpened = false;
    umbrellaMotionState = UMBRELLA_IDLE;
    lastUmbrellaStepMicros = 0;
    motorOff();
  }
}

bool umbrellaIsMoving()
{
  return umbrellaMotionState != UMBRELLA_IDLE;
}

bool umbrellaIsClosing()
{
  return umbrellaMotionState == UMBRELLA_HOMING ||
         umbrellaMotionState == UMBRELLA_SETTLE;
}

void closeUmbrella()
{
  if(!umbrellaOpened && umbrellaMotionState == UMBRELLA_IDLE)
    return;

  startUmbrellaHome();
}

// MAIN ALGORITHMS

void rainTask()
{
  rainDetected = isRainDetected();

  if(rainDetected)
  {
    if(!umbrellaOpened || umbrellaIsClosing())
      openUmbrella();
  }
  else
  {
    if(umbrellaOpened || umbrellaIsMoving())
      closeUmbrella();
  }

  umbrellaMotionTask();
}

void autoTrackingTask()
{
  if(millis() - lastTracking < TRACKING_INTERVAL_MS)
    return;

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

  if(abs(dvert) > tolerance)
  {
    servovert += avt > avd ? stepSize : -stepSize;
    servovert = constrain(servovert, servovertLimitLow, servovertLimitHigh);
    vertical.write(servovert);
  }

  if(abs(dhoriz) > tolerance)
  {
    servohori += avl > avr ? -stepSize : stepSize;
    servohori = constrain(servohori, servohoriLimitLow, servohoriLimitHigh);
    horizontal.write(servohori);
  }
}

void updateStateMachine()
{
  previousState = currentState;
  rainDetected = isRainDetected();

  bool wifiConnected = WiFi.status() == WL_CONNECTED;

  if(wifiConnected)
    wifiEverConnected = true;

  if(!wifiConnected)
  {
    if(wifiEverConnected)
    {
      currentState = STATE_WIFI_LOST;
    }
    else
    {
      currentState = STATE_WIFI_CONNECTING;
    }
  }
  else if(currentMode == MODE_AUTO)
  {
    if(rainDetected)
    {
      currentState = STATE_RAIN_PROTECTION;
    }
    else
    {
      currentState = STATE_AUTO_TRACKING;
    }
  }
  else
  {
    currentState = STATE_MANUAL_CONTROL;
  }

  if(currentState != previousState)
  {
    Serial.print("[STATE] ");
    Serial.print(stateText(previousState));
    Serial.print(" -> ");
    Serial.println(stateText(currentState));
  }
}

void runStateAction()
{
  switch(currentState)
  {
    case STATE_WIFI_CONNECTING:
      wifiTask();
      logSystem();
      break;

    case STATE_WIFI_LOST:
      wifiTask();
      logSystem();
      break;

    case STATE_AUTO_TRACKING:
      wifiTask();
      startCoapServer();
      coap.loop();
      logSystem();
      autoTrackingTask();
      rainTask();
      break;

    case STATE_MANUAL_CONTROL:
      wifiTask();
      startCoapServer();
      coap.loop();
      logSystem();
      umbrellaMotionTask();
      break;

    case STATE_RAIN_PROTECTION:
      wifiTask();
      startCoapServer();
      coap.loop();
      logSystem();
      autoTrackingTask();
      rainTask();
      break;
  }
}

// COAP CALLBACKS

void callbackState(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;

  logCoapRx("state", packet, ip, port, payload);

  if(!verifySecret(packet, data))
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

  if(!verifySecret(packet, data))
  {
    sendCoapText("mode", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  data.toUpperCase();

  if(data == "AUTO")
  {
    currentMode = MODE_AUTO;
    sendCoapText("mode", ip, port, packet.messageid, "OK_AUTO");
  }
  else if(data == "MANUAL")
  {
    currentMode = MODE_MANUAL;
    sendCoapText("mode", ip, port, packet.messageid, "OK_MANUAL");
  }
  else
  {
    sendCoapText("mode", ip, port, packet.messageid, "ERR_MODE");
  }
}

void callbackServo1(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;

  logCoapRx("servo1", packet, ip, port, payload);

  if(!verifySecret(packet, data))
  {
    sendCoapText("servo1", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("servo1", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
    return;
  }

  servovert = constrain(data.toInt(), servovertLimitLow, servovertLimitHigh);
  vertical.write(servovert);

  sendCoapText("servo1", ip, port, packet.messageid, "OK");
}

void callbackServo2(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;

  logCoapRx("servo2", packet, ip, port, payload);

  if(!verifySecret(packet, data))
  {
    sendCoapText("servo2", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("servo2", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
    return;
  }

  servohori = constrain(data.toInt(), servohoriLimitLow, servohoriLimitHigh);
  horizontal.write(servohori);

  sendCoapText("servo2", ip, port, packet.messageid, "OK");
}

void callbackUmbrella(CoapPacket &packet, IPAddress ip, int port)
{
  String payload = getPayload(packet);
  String data;

  logCoapRx("umbrella", packet, ip, port, payload);

  if(!verifySecret(packet, data))
  {
    sendCoapText("umbrella", ip, port, packet.messageid, "ERR_SECRET");
    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("umbrella", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
    return;
  }

  data.toUpperCase();

  if(data == "OPEN")
  {
    openUmbrella();
    sendCoapText("umbrella", ip, port, packet.messageid, "OPEN_OK");
  }
  else if(data == "CLOSE")
  {
    closeUmbrella();
    sendCoapText("umbrella", ip, port, packet.messageid, "CLOSE_OK");
  }
  else
  {
    sendCoapText("umbrella", ip, port, packet.messageid, "ERR_CMD");
  }
}

// SETUP

void setup()
{
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(LDR_TOP_LEFT, ADC_11db);
  analogSetPinAttenuation(LDR_TOP_RIGHT, ADC_11db);
  analogSetPinAttenuation(LDR_BOTTOM_LEFT, ADC_11db);
  analogSetPinAttenuation(LDR_BOTTOM_RIGHT, ADC_11db);
  analogSetPinAttenuation(RAIN_SENSOR_PIN, ADC_11db);

  horizontal.attach(SERVO_HORIZONTAL_PIN);
  vertical.attach(SERVO_VERTICAL_PIN);

  horizontal.write(servohori);
  vertical.write(servovert);

  pinMode(LDR_TOP_LEFT, INPUT);
  pinMode(LDR_TOP_RIGHT, INPUT);
  pinMode(LDR_BOTTOM_LEFT, INPUT);
  pinMode(LDR_BOTTOM_RIGHT, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(LIMIT_SW, INPUT_PULLUP);

  motorOff();

  delay(500);

  homeMotor();
  connectWiFi();
  updateStateMachine();

  Serial.println("[APP] Solar Tracker Started");
}

// LOOP

void loop()
{
  updateStateMachine();
  runStateAction();
}
