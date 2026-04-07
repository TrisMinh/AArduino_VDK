#include <Servo.h>
int aervo =9;
int goc;

Servo s;

void setup() {
  Serial.begin(9600);
  s.attach(aervo);
}

void loop() {
  s.write(0);
  goc = s.read();
  Serial.print("Goc hien tai: "); Serial.println(goc);
  delay(1000);

  s.write(90);
  goc = s.read();
  Serial.print("Goc hien tai: "); Serial.println(goc);
  delay(1000);

  s.write(180);
  goc = s.read();
  Serial.print("Goc hien tai: "); Serial.println(goc);
  delay(1000);
}