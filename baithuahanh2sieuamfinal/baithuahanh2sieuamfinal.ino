int leds[] = {3, 5, 6, 9, 10, 11};  
int n = 6;

const int trigPin = 4;              
const int echoPin = 2;            

long duration;
long distance;

void brightZero() {
  for (int i = 0; i < n; i++) {
    digitalWrite(leds[i], LOW);
  }
}

long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  if (duration == 0) return 0;

  return duration * 0.034 / 2;
}

int convert_delay(long value) {
  value = constrain(value, 5, 50);
  return map(value, 5, 50, 40, 500);
}

void star_rain(int delay_time) {
  int brightness[] = {200, 150, 100};
  int tail = 3;

  for (int m = 0; m < n; m++) {
    brightZero();
	  analogWrite(leds[m], brightness[0]);
    analogWrite(leds[m - 1], brightness[1]);
    analogWrite(leds[m - 2], brightness[2]);

    delay(delay_time);
  }

  brightZero();
}

void setup() {
  for (int i = 0; i < n; i++) {
    pinMode(leds[i], OUTPUT);
  }

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);
}

void loop() {
  distance = getDistance();
  int delay_time = convert_delay(distance);

  star_rain(delay_time);

  Serial.print("Distance:");
  Serial.println(distance);
  Serial.print("Delay:");
  Serial.println(delay_time);
  Serial.println("-----------");
  delay(1000);
}
