#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ESP32Servo.h>

// ================= WIFI =================

const char* WIFI_SSID = "Khu H";
const char* WIFI_PASS = "khuh1234";

String SECRET_KEY = "SUNTRAC123";

// ================= SERVO =================

Servo horizontal;
Servo vertical;

int servohori = 90;
int servovert = 45;

int targetServohori = 90;
int targetServovert = 45;

int servohoriLimitHigh = 170;
int servohoriLimitLow = 10;

int servovertLimitHigh = 80;
int servovertLimitLow = 10;

// ================= LDR =================

#define LDR_TOP_LEFT     33
#define LDR_BOTTOM_LEFT  32
#define LDR_BOTTOM_RIGHT 35
#define LDR_TOP_RIGHT    34

// ================= SERVO PINS =================

#define SERVO_HORIZONTAL_PIN 18
#define SERVO_VERTICAL_PIN   19

// ================= TRACKING =================

int tolerance = 150;
int stepSize = 1;

const unsigned long MANUAL_SERVO_INTERVAL_MS = 25;

// ================= RAIN SENSOR =================

// AO -> VP(GPIO36)

#define RAIN_SENSOR_PIN 36

bool rainDetected = false;

// ================= STEPPER MOTOR =================

#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

#define LIMIT_SW 12

const int STEPS_90_DEG = 1000;

const int STEP_DELAY_MS = 5;
const unsigned long STEP_INTERVAL_US = STEP_DELAY_MS * 1000UL;

const int MAX_HOME_STEPS = 6000;

int currentStep = 0;
long positionSteps = 0;

bool umbrellaOpened = false;

// ================= STEPPER NON-BLOCKING =================

enum UmbrellaMoveState
{
  UMBRELLA_IDLE,
  UMBRELLA_OPENING,
  UMBRELLA_HOMING
};

UmbrellaMoveState umbrellaMoveState = UMBRELLA_IDLE;

bool stepperRunning = false;
int stepperDirection = 1;
long stepperTargetSteps = 0;
long stepperMovedSteps = 0;
unsigned long lastStepMicros = 0;

// ================= SYSTEM =================

unsigned long lastTracking = 0;
unsigned long lastLogTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastManualServoMove = 0;

bool wifiConnectedLogged = false;
bool coapStarted = false;

// ================= STATE MACHINE =================

enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL
};

enum AppState
{
  STATE_WIFI_CONNECTING,
  STATE_WIFI_LOST,
  STATE_AUTO_TRACKING,
  STATE_MANUAL_CONTROL,
  STATE_RAIN_PROTECTION
};

ControlMode currentMode = MODE_AUTO;

AppState currentState = STATE_WIFI_CONNECTING;
AppState previousState = STATE_WIFI_CONNECTING;

// ================= COAP =================

WiFiUDP udp;
Coap coap(udp);

// ================= STEP SEQUENCE =================

const int stepSequence[8][4] =
{
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

// ================= FUNCTION DECLARE =================

void callbackState(CoapPacket &packet,
                   IPAddress ip,
                   int port);

void callbackMode(CoapPacket &packet,
                  IPAddress ip,
                  int port);

void callbackServo1(CoapPacket &packet,
                    IPAddress ip,
                    int port);

void callbackServo2(CoapPacket &packet,
                    IPAddress ip,
                    int port);

void callbackUmbrella(CoapPacket &packet,
                      IPAddress ip,
                      int port);

void motorOff();
bool isHomePressed();

void writeStep(int stepIndex);
void oneStepNoDelay(int direction);
void oneStepBlocking(int direction);

void startStepperMove(long steps,
                      int direction,
                      UmbrellaMoveState moveState);

void stepperTask();
bool homeMotor();
void openUmbrella();
void closeUmbrella();
void manualServoTask();

// ================= MODE TEXT =================

String modeText()
{
  return currentMode == MODE_AUTO ?
         "AUTO" :
         "MANUAL";
}

// ================= STATE TEXT =================

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

// ================= READ LDR =================

int readLDR(int pin)
{
  long total = 0;

  for(int i = 0; i < 10; i++)
  {
    total += analogRead(pin);
  }

  return total / 10;
}

// ================= WIFI =================

void connectWiFi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID,
             WIFI_PASS);

  Serial.println();
  Serial.println("[WIFI] Connecting...");
}

void wifiTask()
{
  if(lastWiFiCheck != 0 &&
     millis() - lastWiFiCheck < 5000)
    return;

  lastWiFiCheck = millis();

  if(WiFi.status() == WL_CONNECTED)
  {
    if(!wifiConnectedLogged)
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

  WiFi.begin(WIFI_SSID,
             WIFI_PASS);
}

// ================= COAP =================

void startCoapServer()
{
  if(coapStarted)
    return;

  coap.server(callbackState,
              "state");

  coap.server(callbackMode,
              "mode");

  coap.server(callbackServo1,
              "servo1");

  coap.server(callbackServo2,
              "servo2");

  coap.server(callbackUmbrella,
              "umbrella");

  coap.start();

  coapStarted = true;

  Serial.println("[COAP] Started");
}

// ================= PAYLOAD =================

String getPayload(CoapPacket &packet)
{
  String payload = "";

  for(int i = 0;
      i < packet.payloadlen;
      i++)
  {
    payload +=
    (char)packet.payload[i];
  }

  payload.trim();

  return payload;
}

bool verifySecret(CoapPacket &packet,
                  String &data)
{
  String payload =
  getPayload(packet);

  int index =
  payload.indexOf(':');

  if(index < 0)
  {
    data = "";

    return false;
  }

  String secret =
  payload.substring(0,
                    index);

  data =
  payload.substring(index + 1);

  data.trim();

  return secret == SECRET_KEY;
}

// ================= COAP LOG =================

void logCoapRx(const char* route,
               CoapPacket &packet,
               IPAddress ip,
               int port,
               const String &payload)
{
  Serial.print("[COAP RX] ");

  Serial.print(route);

  Serial.print(" -> ");

  Serial.println(payload);
}

void sendCoapText(const char* route,
                  IPAddress ip,
                  int port,
                  int messageid,
                  const char* response)
{
  coap.sendResponse(ip,
                    port,
                    messageid,
                    response);

  Serial.print("[COAP TX] ");

  Serial.print(route);

  Serial.print(" -> ");

  Serial.println(response);
}

// ================= JSON =================

String buildStateJson()
{
  String json = "{";

  json += "\"lt\":" +
          String(readLDR(LDR_TOP_LEFT)) + ",";

  json += "\"rt\":" +
          String(readLDR(LDR_TOP_RIGHT)) + ",";

  json += "\"ld\":" +
          String(readLDR(LDR_BOTTOM_LEFT)) + ",";

  json += "\"rd\":" +
          String(readLDR(LDR_BOTTOM_RIGHT)) + ",";

  json += "\"v\":" +
          String(servovert) + ",";

  json += "\"h\":" +
          String(servohori) + ",";

  json += "\"rain\":" +
          String(rainDetected ? 1 : 0) + ",";

  json += "\"umbrella\"";

  json += umbrellaOpened ?
          "OPEN" :
          "CLOSE";

  json += "\"}";

  return json;
}

// ================= STEPPER =================

void writeStep(int stepIndex)
{
  digitalWrite(IN1,
               stepSequence[stepIndex][0]);

  digitalWrite(IN2,
               stepSequence[stepIndex][1]);

  digitalWrite(IN3,
               stepSequence[stepIndex][2]);

  digitalWrite(IN4,
               stepSequence[stepIndex][3]);
}

void oneStepNoDelay(int direction)
{
  currentStep += direction;

  if(currentStep > 7)
    currentStep = 0;

  if(currentStep < 0)
    currentStep = 7;

  writeStep(currentStep);

  positionSteps += direction;
}

void oneStepBlocking(int direction)
{
  oneStepNoDelay(direction);

  delayMicroseconds(STEP_INTERVAL_US);
}

void startStepperMove(long steps,
                      int direction,
                      UmbrellaMoveState moveState)
{
  if(stepperRunning)
    return;

  stepperRunning = true;
  stepperDirection = direction;
  stepperTargetSteps = steps;
  stepperMovedSteps = 0;
  umbrellaMoveState = moveState;
  lastStepMicros = micros();
}

void stepperTask()
{
  if(!stepperRunning)
    return;

  if(micros() - lastStepMicros < STEP_INTERVAL_US)
    return;

  lastStepMicros = micros();

  if(umbrellaMoveState == UMBRELLA_HOMING)
  {
    if(isHomePressed())
    {
      stepperRunning = false;
      umbrellaOpened = false;
      positionSteps = 0;
      umbrellaMoveState = UMBRELLA_IDLE;

      motorOff();

      Serial.println("[UMBRELLA] Home OK");

      return;
    }
  }

  oneStepNoDelay(stepperDirection);

  stepperMovedSteps++;

  if(stepperMovedSteps >= stepperTargetSteps)
  {
    stepperRunning = false;

    if(umbrellaMoveState == UMBRELLA_OPENING)
    {
      umbrellaOpened = true;
    }
    else if(umbrellaMoveState == UMBRELLA_HOMING)
    {
      Serial.println("[UMBRELLA] Home ERROR");
    }

    umbrellaMoveState = UMBRELLA_IDLE;

    motorOff();
  }
}

void motorOff()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

bool isHomePressed()
{
  if(digitalRead(LIMIT_SW) == LOW)
  {
    delay(20);

    return digitalRead(LIMIT_SW)
           == LOW;
  }

  return false;
}

bool homeMotor()
{
  Serial.println("[UMBRELLA] Homing");

  int count = 0;

  while(!isHomePressed() &&
        count < MAX_HOME_STEPS)
  {
    oneStepBlocking(-1);

    count++;
  }

  if(!isHomePressed())
  {
    Serial.println(
    "[UMBRELLA] Home ERROR");

    motorOff();

    return false;
  }

  positionSteps = 0;

  Serial.println(
  "[UMBRELLA] Home OK");

  delay(300);

  return true;
}

void openUmbrella()
{
  if(umbrellaOpened)
    return;

  if(stepperRunning)
    return;

  Serial.println(
  "[UMBRELLA] OPEN");

  startStepperMove(STEPS_90_DEG,
                   1,
                   UMBRELLA_OPENING);
}

void closeUmbrella()
{
  if(!umbrellaOpened)
    return;

  if(stepperRunning)
    return;

  Serial.println(
  "[UMBRELLA] CLOSE");

  startStepperMove(MAX_HOME_STEPS,
                   -1,
                   UMBRELLA_HOMING);
}

// ================= RAIN TASK =================

void rainTask()
{
  int rainValue =
  analogRead(RAIN_SENSOR_PIN);

  static bool rainState = false;

  Serial.print("Rain Analog: ");

  Serial.println(rainValue);

  // hysteresis chống nhấp nháy

  if(rainValue < 2200)
  {
    rainState = true;
  }
  else if(rainValue > 3000)
  {
    rainState = false;
  }

  rainDetected = rainState;

  if(rainDetected)
  {
    openUmbrella();
  }
  else
  {
    closeUmbrella();
  }
}

// ================= AUTO TRACKING =================

void autoTrackingTask()
{
  if(millis() - lastTracking < 30)
    return;

  lastTracking = millis();

  int lt =
  readLDR(LDR_TOP_LEFT);

  int rt =
  readLDR(LDR_TOP_RIGHT);

  int ld =
  readLDR(LDR_BOTTOM_LEFT);

  int rd =
  readLDR(LDR_BOTTOM_RIGHT);

  int avt = (lt + rt) / 2;
  int avd = (ld + rd) / 2;

  int avl = (lt + ld) / 2;
  int avr = (rt + rd) / 2;

  int dvert = avt - avd;
  int dhoriz = avl - avr;

  if(abs(dvert) > tolerance)
  {
    servovert +=
    avt > avd ?
    stepSize :
    -stepSize;

    servovert =
    constrain(servovert,
              servovertLimitLow,
              servovertLimitHigh);

    vertical.write(servovert);
  }

  if(abs(dhoriz) > tolerance)
  {
    servohori +=
    avl > avr ?
    -stepSize :
    stepSize;

    servohori =
    constrain(servohori,
              servohoriLimitLow,
              servohoriLimitHigh);

    horizontal.write(servohori);
  }
}

// ================= MANUAL SERVO SMOOTH =================

void manualServoTask()
{
  if(millis() - lastManualServoMove < MANUAL_SERVO_INTERVAL_MS)
    return;

  lastManualServoMove = millis();

  if(servovert != targetServovert)
  {
    servovert +=
    targetServovert > servovert ?
    1 :
    -1;

    servovert =
    constrain(servovert,
              servovertLimitLow,
              servovertLimitHigh);

    vertical.write(servovert);
  }

  if(servohori != targetServohori)
  {
    servohori +=
    targetServohori > servohori ?
    1 :
    -1;

    servohori =
    constrain(servohori,
              servohoriLimitLow,
              servohoriLimitHigh);

    horizontal.write(servohori);
  }
}

// ================= STATE MACHINE =================

void updateStateMachine()
{
  previousState = currentState;

  int rainValue =
  analogRead(RAIN_SENSOR_PIN);

  rainDetected =
  rainValue < 2200;

  if(WiFi.status() != WL_CONNECTED)
  {
    if(wifiConnectedLogged)
    {
      currentState =
      STATE_WIFI_LOST;
    }
    else
    {
      currentState =
      STATE_WIFI_CONNECTING;
    }
  }
  else if(currentMode == MODE_AUTO)
  {
    if(rainDetected)
    {
      currentState =
      STATE_RAIN_PROTECTION;
    }
    else
    {
      currentState =
      STATE_AUTO_TRACKING;
    }
  }
  else
  {
    currentState =
    STATE_MANUAL_CONTROL;
  }

  if(currentState != previousState)
  {
    Serial.print("[STATE] ");

    Serial.print(
    stateText(previousState));

    Serial.print(" -> ");

    Serial.println(
    stateText(currentState));
  }
}

// ================= LOG =================

void logSystem()
{
  if(millis() - lastLogTime < 1000)
    return;

  lastLogTime = millis();

  Serial.println();

  Serial.println(
  "========== SYSTEM ==========");

  Serial.print("Mode: ");

  Serial.println(modeText());

  Serial.print("State: ");

  Serial.println(
  stateText(currentState));

  Serial.print("Rain: ");

  Serial.println(rainDetected);

  Serial.print("Rain Analog: ");

  Serial.println(
  analogRead(RAIN_SENSOR_PIN));

  Serial.print("Umbrella: ");

  Serial.println(umbrellaOpened);

  Serial.println(
  "============================");
}

// ================= RUN STATE =================

void runStateAction()
{
  stepperTask();

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

      manualServoTask();

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

// ================= COAP CALLBACK =================

void callbackState(CoapPacket &packet,
                   IPAddress ip,
                   int port)
{
  String payload =
  getPayload(packet);

  String data;

  logCoapRx("state",
            packet,
            ip,
            port,
            payload);

  if(!verifySecret(packet,
                   data))
  {
    sendCoapText("state",
                 ip,
                 port,
                 packet.messageid,
                 "ERR_SECRET");

    return;
  }

  String response =
  buildStateJson();

  sendCoapText("state",
               ip,
               port,
               packet.messageid,
               response.c_str());
}

void callbackMode(CoapPacket &packet,
                  IPAddress ip,
                  int port)
{
  String payload =
  getPayload(packet);

  String data;

  logCoapRx("mode",
            packet,
            ip,
            port,
            payload);

  if(!verifySecret(packet,
                   data))
  {
    sendCoapText("mode",
                 ip,
                 port,
                 packet.messageid,
                 "ERR_SECRET");

    return;
  }

  data.toUpperCase();

  if(data == "AUTO")
  {
    currentMode = MODE_AUTO;
    targetServovert = servovert;
    targetServohori = servohori;

    sendCoapText("mode",
                 ip,
                 port,
                 packet.messageid,
                 "OK_AUTO");
  }
  else if(data == "MANUAL")
  {
    currentMode = MODE_MANUAL;
    targetServovert = servovert;
    targetServohori = servohori;

    sendCoapText("mode",
                 ip,
                 port,
                 packet.messageid,
                 "OK_MANUAL");
  }
}

void callbackServo1(CoapPacket &packet,
                    IPAddress ip,
                    int port)
{
  String payload =
  getPayload(packet);

  String data;

  if(!verifySecret(packet,
                   data))
  {
    sendCoapText("servo1",
                 ip,
                 port,
                 packet.messageid,
                 "ERR_SECRET");

    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("servo1",
                 ip,
                 port,
                 packet.messageid,
                 "AUTO_MODE_ACTIVE");

    return;
  }

  targetServovert =
  constrain(data.toInt(),
            servovertLimitLow,
            servovertLimitHigh);

  sendCoapText("servo1",
               ip,
               port,
               packet.messageid,
               "OK");
}

void callbackServo2(CoapPacket &packet,
                    IPAddress ip,
                    int port)
{
  String payload =
  getPayload(packet);

  String data;

  if(!verifySecret(packet,
                   data))
  {
    sendCoapText("servo2",
                 ip,
                 port,
                 packet.messageid,
                 "ERR_SECRET");

    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("servo2",
                 ip,
                 port,
                 packet.messageid,
                 "AUTO_MODE_ACTIVE");

    return;
  }

  targetServohori =
  constrain(data.toInt(),
            servohoriLimitLow,
            servohoriLimitHigh);

  sendCoapText("servo2",
               ip,
               port,
               packet.messageid,
               "OK");
}

void callbackUmbrella(CoapPacket &packet,
                      IPAddress ip,
                      int port)
{
  String payload =
  getPayload(packet);

  String data;

  if(!verifySecret(packet,
                   data))
  {
    sendCoapText("umbrella",
                 ip,
                 port,
                 packet.messageid,
                 "ERR_SECRET");

    return;
  }

  if(currentMode != MODE_MANUAL)
  {
    sendCoapText("umbrella",
                 ip,
                 port,
                 packet.messageid,
                 "AUTO_MODE_ACTIVE");

    return;
  }

  data.toUpperCase();

  if(data == "OPEN")
  {
    openUmbrella();

    sendCoapText("umbrella",
                 ip,
                 port,
                 packet.messageid,
                 "OPEN_OK");
  }
  else if(data == "CLOSE")
  {
    closeUmbrella();

    sendCoapText("umbrella",
                 ip,
                 port,
                 packet.messageid,
                 "CLOSE_OK");
  }
}

// ================= SETUP =================

void setup()
{
  Serial.begin(115200);

  analogReadResolution(12);

  analogSetAttenuation(ADC_11db);

  horizontal.attach(
  SERVO_HORIZONTAL_PIN);

  vertical.attach(
  SERVO_VERTICAL_PIN);

  horizontal.write(servohori);

  vertical.write(servovert);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(LIMIT_SW,
          INPUT_PULLUP);

  motorOff();

  delay(500);

  homeMotor();

  connectWiFi();

  updateStateMachine();

  Serial.println(
  "[APP] Solar Tracker Started");
}

// ================= LOOP =================

void loop()
{
  updateStateMachine();

  runStateAction();
}
