const int ledPins[8] = {6, 7, 8, 9, 10, 11, 12, 13};
int n = 8;

volatile unsigned long lastTimeA = 0;
volatile unsigned long lastTimeB = 0;
const unsigned long debounceTime = 200; 

volatile bool runH1 = false;
volatile bool runH2 = false;

volatile unsigned long countBtn1 = 0;
volatile unsigned long countBtn2 = 0;

volatile bool evtBtn1 = false;
volatile bool evtBtn2 = false;

void setup() {
  for (int i = 0; i < n; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  pinMode(2, INPUT_PULLUP); 
  pinMode(3, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(2), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), isrB, FALLING);

  Serial.begin(9600);
}

void H1() {
  for (int pos = 0; pos < n && runH1 == true; pos++) {
    clearAll();
    digitalWrite(ledPins[pos], HIGH);
    digitalWrite(ledPins[(pos - 1)], HIGH);
    digitalWrite(ledPins[(pos - 2)], HIGH);
    delay(150);
  }
  clearAll();
  runH1 = false;
}

void H2() {
  int i = 1;

  while (i <= n && runH2 == true) {
    clearAll();
    for (int j = 0; j < i; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(150);
    i++;
  }

  i = n-1;
  while (i >= 1 && runH2 == true) {
    clearAll();
    for (int j = 0; j < i; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(150);
    i--;
  }

  clearAll();
  runH2 = false;
}

void clearAll() {
  for (int i = 0; i < n; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

void isrA() {
  unsigned long now = millis();
  if (now - lastTimeA < debounceTime) return;
  lastTimeA = now;
  runH1 = !runH1;
  runH2 = false;
  evtBtn1 = true;
  countBtn1++;
}

void isrB() {
  unsigned long now = millis();
  if (now - lastTimeB < debounceTime) return;
  lastTimeB = now;
  runH2 = !runH2;
  runH1 = false;
  evtBtn2 = true;
  countBtn2++;
}

void loop() {
  if (evtBtn1) {
    evtBtn1 = false;
    Serial.print("Nut 1: ");
    Serial.println(countBtn1);
  }
  if (evtBtn2) {
    evtBtn2 = false;
    Serial.print("Nut 2: ");
    Serial.println(countBtn2);
  }
  if (runH1) {
    H1();
  }
  if (runH2) {
    H2();
  }

  if (!runH1 && !runH2) {
    Serial.println("Toi dang cho ban nhan nut");
    delay(1000);
  }
}

