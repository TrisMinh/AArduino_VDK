// C++ code
//
int pin = 9;
int analogPot = A0;

void setup()
{
  pinMode(pin, OUTPUT);
  Serial.begin(9600);
}

void loop()
{
  int sensorValue = analogRead(analogPot);
  int brightness = sensorValue /4 ; //0-1023 ~ 0-255
  analogWrite(pin , brightness);
  
  Serial.print("Gia tri bien tro: ");
  Serial.print(sensorValue);
  Serial.print("	|	Cuong do sang led: ");
  Serial.print(brightness);
  Serial.println();
  delay(200);
}