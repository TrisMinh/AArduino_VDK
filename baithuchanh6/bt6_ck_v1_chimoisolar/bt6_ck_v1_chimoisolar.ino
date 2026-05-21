#include <Servo.h>

// ===== Servo =====
Servo horizontal;
Servo vertical;

int servohori = 90;
int servovert = 45;

int servohoriLimitHigh = 170;
int servohoriLimitLow = 10;

int servovertLimitHigh = 80;
int servovertLimitLow = 10;

// ===== LDR =====
#define LDR_TOP_LEFT     A0
#define LDR_BOTTOM_LEFT  A1
#define LDR_BOTTOM_RIGHT A2
#define LDR_TOP_RIGHT    A3

// ===== Servo pins =====
#define SERVO_HORIZONTAL_PIN 9
#define SERVO_VERTICAL_PIN   10

// ===== Tracking =====
int tolerance = 100;
int stepSize = 2;

void setup()
{
  Serial.begin(9600);

  horizontal.attach(SERVO_HORIZONTAL_PIN);
  vertical.attach(SERVO_VERTICAL_PIN);

  horizontal.write(servohori);
  vertical.write(servovert);

  delay(1000);
}

void loop()
{
  // Đọc cảm biến ánh sáng
  int lt = analogRead(LDR_TOP_LEFT);
  int rt = analogRead(LDR_TOP_RIGHT);
  int ld = analogRead(LDR_BOTTOM_LEFT);
  int rd = analogRead(LDR_BOTTOM_RIGHT);

  // Tính trung bình
  int avt = (lt + rt) / 2;
  int avd = (ld + rd) / 2;
  int avl = (lt + ld) / 2;
  int avr = (rt + rd) / 2;

  // Sai lệch
  int dvert = avt - avd;
  int dhoriz = avl - avr;

  // ===== Servo dọc =====
  if (abs(dvert) > tolerance)
  {
    if (avt > avd)
    {
      servovert += stepSize;
    }
    else
    {
      servovert -= stepSize;
    }

    servovert = constrain(servovert,
                           servovertLimitLow,
                           servovertLimitHigh);

    vertical.write(servovert);
  }

  // ===== Servo ngang =====
  if (abs(dhoriz) > tolerance)
  {
    if (avl > avr)
    {
      servohori -= stepSize;
    }
    else
    {
      servohori += stepSize;
    }

    servohori = constrain(servohori,
                           servohoriLimitLow,
                           servohoriLimitHigh);

    horizontal.write(servohori);
  }

  // Debug Serial
  Serial.print("V: ");
  Serial.print(servovert);

  Serial.print(" | H: ");
  Serial.print(servohori);

  Serial.print(" | LT: ");
  Serial.print(lt);

  Serial.print(" | RT: ");
  Serial.print(rt);

  Serial.print(" | LD: ");
  Serial.print(ld);

  Serial.print(" | RD: ");
  Serial.println(rd);

  delay(30);
}