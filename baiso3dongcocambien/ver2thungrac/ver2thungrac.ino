#include <Servo.h>

int servoPin = 9;
int goc;

Servo servo;

const int trigPin = 4;
const int echoPin = 2;


long duration;
long distance;

bool doorOpen = false;   
bool first = false;

long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    Serial.println("Khong co vat can trong tam");
    return 999;
  }

  long d = duration * 0.034 / 2; // đổi đơn vị thành cm/micros

  Serial.print("Distance: ");
  Serial.print(d);
  Serial.println(" cm");

  return d;
}

void resetdoor() {
  Serial.println(">> DONG CUA");
  servo.write(0);
  goc = servo.read();
  Serial.print("Goc hien tai: ");
  Serial.println(goc);
  doorOpen = false;
}

void opendoor() {
  Serial.println(">> MO CUA");
  servo.write(90);
  goc = servo.read();
  Serial.print("Goc hien tai: "); Serial.println(goc);
  
  doorOpen = true;

  delay(5000);

  while (true) {
    distance = getDistance();
    if (distance >= 50) break;

    Serial.println("Vat van o gan -> Giu mo");
    delay(1000);
  }

  resetdoor();
}

bool validDistance() {
  if(distance > 5 && distance < 50) return true;
  else return false;
}

void setup() {
  servo.attach(servoPin);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);
  Serial.println("=== BAT DAU ===");
}

void loop() {
  if (!first) 
  {
    resetdoor();
    first = true;
  }
  distance = getDistance();
  Serial.println(distance);
  if(validDistance() && !doorOpen) {
    opendoor();
  }
  delay(1000);
}