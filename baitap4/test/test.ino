#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT22

#define RELAY_PIN 8
#define BUZZER_PIN 7

#define TEMP_LIMIT 32
#define HUMI_LIMIT 90

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

unsigned long alarmStart = 0;
unsigned long buzzerTimer = 0;

bool overThreshold = false;
bool buzzerState = false;

float humi;
float tempC;
bool fanOn = false;

void setup() {

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  dht.begin();

  lcd.setCursor(0,0);
  lcd.print("System Start");
  lcd.setCursor(0,1);
  lcd.print("Wait Sensor");

  delay(2000);
  lcd.clear();
}

void readSensor(){

  humi  = dht.readHumidity();
  tempC = dht.readTemperature();

  if (isnan(humi) || isnan(tempC)) {

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Sensor Error");

    Serial.println("Sensor Error");

    delay(2000);
  }

}

void controlFan(){

  if(humi > HUMI_LIMIT || tempC > TEMP_LIMIT){
    fanOn = true;
  }
  else{
    fanOn = false;
  }

  if(fanOn){
    digitalWrite(RELAY_PIN, HIGH);
  }
  else{
    digitalWrite(RELAY_PIN, LOW);
  }

}

void controlAlarm(){

  if(fanOn){

    if(!overThreshold){
      alarmStart = millis();
      overThreshold = true;
    }

    if(millis() - alarmStart > 10000){

      if(millis() - buzzerTimer > 500){
        buzzerTimer = millis();
        buzzerState = !buzzerState;

        digitalWrite(BUZZER_PIN, !buzzerState);
      }

    }

  }
  else{

    overThreshold = false;
    digitalWrite(BUZZER_PIN, HIGH);

  }

}

void displayLCD(){

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(tempC);
  lcd.print("C ");

  lcd.print("H:");
  lcd.print(humi);
  lcd.print("%");

  lcd.setCursor(0,1);

  if(millis() - alarmStart > 10000 && fanOn){
    lcd.print("!!! ALARM !!!");
  }
  else if(fanOn){
    lcd.print("Fan:ON ");
  }
  else{
    lcd.print("Fan:OFF");
  }

}

void printSerial(){

  Serial.print("Temp: ");
  Serial.print(tempC);
  Serial.print(" C  ");

  Serial.print("Humidity: ");
  Serial.print(humi);
  Serial.print(" %  ");

  Serial.print("Fan: ");

  if(fanOn){
    Serial.println("ON");
  }
  else{
    Serial.println("OFF");
  }

}

void loop() {

  readSensor();

  controlFan();

  controlAlarm();

  printSerial();

  displayLCD();

  delay(2000);
}