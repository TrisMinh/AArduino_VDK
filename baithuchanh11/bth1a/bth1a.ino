const byte ledPins[8] = {6, 7, 8, 9, 10, 11, 12, 13};
int n = 8;

volatile bool runH1 = false;
volatile bool runH2 = false;

void setup() {
  for (int i = 0; i < n; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  pinMode(2, INPUT_PULLUP); 
  pinMode(3, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(2), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), isrB, FALLING);
}

void H1() {
  for (int pos = 0; pos < n; pos++) {
    clearAll();
    if(runH2 == true) {
    	runH1 = false;
      	return;
    }

    digitalWrite(ledPins[pos], HIGH);
    digitalWrite(ledPins[(pos - 1)], HIGH);
    digitalWrite(ledPins[(pos - 2)], HIGH);

    delay(150);
  }

  clearAll();
}

void H2() {
  int i = 1;

  while (i <= n) {
    clearAll();
    if(runH1 == true) {
    	runH2 = false;
      	return;
    }
    for (int j = 0; j < i; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(150);
    i++;
  }

  i = n-1;
  while (i >= 1) {
    clearAll();
	if(runH1 == true) {
    	runH2 = false;
      	return;
    }
    for (int j = 0; j < i; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(150);
    i--;
  }

  clearAll();
}

void clearAll() {
  for (byte i = 0; i < n; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

void isrA() {
  runH1 = true;
}

void isrB() {
  runH2 = true;
}

void loop() {
  if (runH1) {
    H1();
    runH1 = false;
  }

  if (runH2) {
    H2();
    runH2 = false;
  }
}

