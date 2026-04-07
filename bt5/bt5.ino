#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHTesp.h>

#define DHT_PIN 15
#define RELAY_PIN 18
#define BUZZER_PIN 19

DHTesp dht;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 DHT22 Test");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // test coi luc khoi dong
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);

  delay(1500);
}

void loop() {
  TempAndHumidity data = dht.getTempAndHumidity();

  if (isnan(data.temperature) || isnan(data.humidity)) {
    Serial.println("Read failed from DHT22!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DHT22 Error");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");

    // bao loi bang coi
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(1500);
    return;
  }

  float temp = data.temperature;
  float hum = data.humidity;

  Serial.println("----- SENSOR DATA -----");
  Serial.print("Temperature: ");
  Serial.print(temp, 1);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(hum, 1);
  Serial.println(" %");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(hum, 1);
  lcd.print("%");

  if (temp > 30) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay ON");

    // coi keu khi nhiet do > 30
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Relay OFF");
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(2000);
}