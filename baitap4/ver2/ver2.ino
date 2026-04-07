
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
// chinh lai pin
#define DHTPIN 15
#define DHTTYPE DHT22

#define RELAY_PIN 18
#define BUZZER_PIN 19

#define TEMP_LIMIT 30
#define HUMI_LIMIT 85

#define TEMP_ALARM 32
#define HUMI_ALARM 85

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2); // địa chỉ lcd

unsigned long alarm_time_start = 0;
unsigned long buzzer_time_start = 0;
unsigned long fan_timer_start = 0;

bool fanOn = false;
bool buzzerOn = false;
bool alarmOn = false;
bool fanWaiting = false;
bool vuotnguong = false;

float temp;
float humi;

bool errorSensor = false;
int errorSensorCount = 0;

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight(); 
  lcd.setCursor(0,0);// CỘT TRƯỚC HÀNG SAU
  lcd.print("SYSTEM START");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  dht.begin();// ở trên định nghĩa DHT rồi, h begin lib sẽ tự gắn pinmode

  delay(2000);
  lcd.clear();
}

void readSensor() {
  // tạo biến mới, khi lỗi biến cũ không đổi, tránh lỗi giả
  float newTemp = dht.readTemperature();
  float newHumi = dht.readHumidity();
  // sẽ có lúc sensor đọc lỗi
  if (isnan(newTemp) || isnan(newHumi)) {
    if(errorSensorCount < 100) { // giới hạn lỡ đếm quá sẽ tràn
      errorSensorCount++;
    }

    if(errorSensorCount >= 5){
      errorSensor = true;
    }

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("SENSOR ERROR");
    Serial.println("Sensor Error");
    return;
  }
  temp = newTemp;
  humi = newHumi;

  errorSensorCount = 0;
  errorSensor = false;
}

void controlFan() {
  if(errorSensor){
    fanOn = true;
    digitalWrite(RELAY_PIN, HIGH);
    return;
  }

  if(temp > TEMP_LIMIT || humi > HUMI_LIMIT){
    if(!fanWaiting){
      fanWaiting = true;
      fan_timer_start = millis();
    }
    // sau 5 giây mới bật quạt
    if(millis() - fan_timer_start > 5000){
      fanOn = true;
    }
  }
  else{
    fanWaiting = false;
    fanOn = false;
  }
  
  if (fanOn) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
}

void controlBuzzer() {
  //sensor lỗi thì kêu liên tục
  if(errorSensor){
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  if (temp > TEMP_ALARM || humi > HUMI_ALARM) {
    if (!vuotnguong) {
      vuotnguong = true;
      alarm_time_start = millis();
      buzzer_time_start = millis();
    }

    if (millis() - alarm_time_start > 10000) {
      alarmOn = true;

      if (millis() - buzzer_time_start > 500) {
        buzzer_time_start = millis();
        buzzerOn = !buzzerOn;
        digitalWrite(BUZZER_PIN, !buzzerOn);
      }
    }

  } 
  else {
    vuotnguong = false;
    alarmOn = false;

    digitalWrite(BUZZER_PIN, HIGH);
  }
}

void printSerial() {
  if(errorSensor){
    Serial.println("Sensor Error!!");
    return;
  }
  Serial.print("Temp: "); Serial.print(temp); Serial.print(" C  ");
  Serial.print("Humidity: "); Serial.print(humi); Serial.print(" %  ");

  Serial.print("Fan: ");

  if(fanOn){
    Serial.println("ON");
  } else{
    Serial.println("OFF");
  }
}

void displayLCD() {
  if(errorSensor){
    return;
  }
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("T:"); lcd.print(temp); lcd.print("C ");
  lcd.print("H:"); lcd.print(humi); lcd.print("%");

  lcd.setCursor(0,1);

  if (alarmOn) {
    lcd.print("!!! ALARM !!!");
  }
  else if(fanOn) {
    lcd.print("Fan:ON ");
  }
  else{
    lcd.print("Fan:OFF");
  }
}

void loop() {
  readSensor();
  controlFan();
  controlBuzzer();
  printSerial();
  displayLCD();
  delay(2000);
}