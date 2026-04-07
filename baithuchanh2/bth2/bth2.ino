int leds[] = {11, 10, 9, 6, 5, 3};
const int n = sizeof(leds) / sizeof(leds[0]);
const int analogPot = A0;

void brightZero() {
  for (int i = 0; i < n; i++) {
    analogWrite(leds[i], 0);
  }
}

int convert_delay(int value) {
  return 30 + value / 4;   // 30 - 285 ms
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

  star_rain(delay_time);

  Serial.print("Gia tri bien tro: ");
  Serial.print(sensorValue);
  Serial.print(" | Toc do delay: ");
  Serial.println(delay_time);
}
