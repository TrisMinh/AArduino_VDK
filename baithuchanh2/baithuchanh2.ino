int leds[] = {11, 10, 9, 6, 5, 3};
const int n = 6;
const int analogPot = A0;

void brightZero() {
  for (int i = 0; i < n; i++) {
    analogWrite(leds[i], 0);
  }
}

int convert_delay(int value) {
  return 30 + value / 4;  
}

void star_rain(int delay_time) {
  int brightness[] = {100, 50, 30};
  int tail = 3;

  for (int m = 0; m < n; m++) {
    brightZero();

    for (int i = 0; i < tail && (m - i) >= 0; i++) {
      analogWrite(leds[m - i], brightness[i]);
    }

    delay(delay_time);
  }

  brightZero();
}

void setup() {
  for (int i = 0; i < n; i++) {
    pinMode(leds[i], OUTPUT);
  }
  Serial.begin(9600);
}

void loop() {
  int sensorValue = analogRead(analogPot);
  int delay_time = convert_delay(sensorValue);
  float dienap = sensorValue * 5.0 / 1023.0;

  star_rain(delay_time);

  Serial.print("Bac bien tro: ");
  Serial.println(sensorValue);

  Serial.print("Voltage:");
  Serial.println(dienap);
  Serial.print("Delaytime:");
  Serial.println(delay_time);
  Serial.println("------------------------");
  delay(1000);
}
