/* =================== GLOBAL =================== */

volatile byte mode = 0;
// 0: OFF, 1: H1, 2: H2

int pin[] = {13, 12, 11, 10, 9, 8, 7, 6};
const byte n = 8;

int button1 = 3;
int button2 = 2;

volatile unsigned long t1 = 0, t2 = 0;
const unsigned long db = 200;

/* =================== UTILS =================== */

void allOff() {
  byte i = 0;
  while (i < n) {
    digitalWrite(pin[i], LOW);
    i++;
  }
}

/* =================== EFFECT H1 =================== */
/* Sao băng 3 LED */

void H1() {
  int i = 0;

  while (mode == 1) {
    allOff();

    digitalWrite(pin[i % n], HIGH);
    digitalWrite(pin[(i + 1) % n], HIGH);
    digitalWrite(pin[(i + 2) % n], HIGH);

    delay(100);
    i = (i + 1) % n;
  }

  allOff();
}

/* =================== EFFECT H2 =================== */
/* Sáng dần → tắt dần (NO if) */

void H2() {
  int i = 0;

  allOff();

  // ====== SÁNG DẦN ======
  while (mode == 2 && i < n) {
    digitalWrite(pin[i], HIGH);
    delay(200);
    i++;
  }

  // ====== TẮT DẦN ======
  i = n - 1;
  while (mode == 2 && i >= 0) {
    digitalWrite(pin[i], LOW);
    delay(200);
    i--;
  }

  allOff();
}


/* =================== FSM TABLE =================== */

void (*effects[])() = {
  nullptr, // mode 0
  H1,      // mode 1
  H2       // mode 2
};

/* =================== ISR =================== */

void isr1() {
  unsigned long now = millis();
  bool ok = (now - t1) >= db;
  t1 = ok ? now : t1;

  mode = ok ? (mode == 1 ? 0 : 1) : mode;
}

void isr2() {
  unsigned long now = millis();
  bool ok = (now - t2) >= db;
  t2 = ok ? now : t2;

  mode = ok ? (mode == 2 ? 0 : 2) : mode;
}

/* =================== SETUP =================== */

void setup() {
  byte i = 0;
  while (i < n) {
    pinMode(pin[i], OUTPUT);
    i++;
  }

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(button1), isr1, FALLING);
  attachInterrupt(digitalPinToInterrupt(button2), isr2, FALLING);

  allOff();
}

/* =================== LOOP =================== */

void loop() {
  void (*fx)() = effects[mode];

  while (fx) {
    fx();
    fx = effects[mode];
  }
}
