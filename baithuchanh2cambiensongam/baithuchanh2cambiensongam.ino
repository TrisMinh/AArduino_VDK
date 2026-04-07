int leds[] = {11, 10, 9, 6, 5, 3};
int n = 6;
int analogPot = A0;

void setup() {
  for (int i = 0; i < n; i++) {
    pinMode(leds[i], OUTPUT);
  }
  Serial.begin(9600);
}

void loop() {
  int sensorValue = analogRead(analogPot);  
  int brightness = sensorValue / 4;        
  float dienap = sensorValue * 5.0 / 1023.0;

  for (int i = 0; i < n; i++) {
    analogWrite(leds[i], brightness);
  }

  Serial.print(" ADC: ");
  Serial.println(sensorValue);

  Serial.print(" Voltage:");
  Serial.println(dienap);
  Serial.print(" Brightness:");
  Serial.println(brightness);

  delay(1000);
}
