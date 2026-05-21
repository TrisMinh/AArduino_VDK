#include <Servo.h>

Servo horizontal;
Servo vertical;

// =====================
// START POSITION
// =====================
int servohori = 180;
int servovert = 45;

// =====================
// LIMIT
// =====================
int servohoriLimitHigh = 175;
int servohoriLimitLow = 5;

int servovertLimitHigh = 100;
int servovertLimitLow = 1;

// =====================
// LDR
// =====================
int ldrlt = A0; // Top Left
int ldrrt = A3; // Top Right
int ldrld = A1; // Bottom Left
int ldrrd = A2; // Bottom Right

void setup()
{
  horizontal.attach(9); // ngang
  vertical.attach(10);  // dọc

  horizontal.write(servohori);
  vertical.write(servovert);

  delay(2500);
}

void loop()
{
  int lt = analogRead(ldrlt);
  int rt = analogRead(ldrrt);
  int ld = analogRead(ldrld);
  int rd = analogRead(ldrrd);

  int dtime = 20;
  int tol = 90;

  // =====================
  // AVERAGE
  // =====================
  int avt = (lt + rt) / 2;
  int avd = (ld + rd) / 2;

  int avl = (lt + ld) / 2;
  int avr = (rt + rd) / 2;

  // =====================
  // DIFFERENCE
  // =====================
  int dvert = avt - avd;
  int dhoriz = avl - avr;

  // =====================
  // VERTICAL
  // =====================
  if (abs(dvert) > tol)
  {
    if (avt > avd)
    {
      servovert++;
    }
    else
    {
      servovert--;
    }

    servovert = constrain(
      servovert,
      servovertLimitLow,
      servovertLimitHigh
    );

    vertical.write(servovert);
  }

  // =====================
  // HORIZONTAL
  // =====================
  if (abs(dhoriz) > tol)
  {
    if (avl > avr)
    {
      servohori--;
    }
    else
    {
      servohori++;
    }

    servohori = constrain(
      servohori,
      servohoriLimitLow,
      servohoriLimitHigh
    );

    horizontal.write(servohori);
  }

  delay(dtime);
}