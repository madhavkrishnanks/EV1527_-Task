const int txPin = 8;
const uint32_t code = 0x3802063C;   // 24 bits are transmitted: 0x02063C
const int repeats = 4;

const int T = 250;
const int T0H = T,     T0L = 3 * T;
const int T1H = 3 * T, T1L = T;
const int TSH = T,     TSL = 31 * T;

void setup() {
  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);
  Serial.begin(9600);
}

void loop() {
  for (int r = 0; r < repeats; r++) {
    sendCode(code);
    sendSync();
    delay(10);
  }
  delay(1000);          // commented out for the link and relay tests
}

void sendCode(uint32_t code) {
  for (int i = 23; i >= 0; i--) {      // 24 bits, MSB first
    sendBit((code >> i) & 1);
  }
}

void sendBit(bool bit) {
  if (bit) {
    digitalWrite(txPin, HIGH); delayMicroseconds(T1H);
    digitalWrite(txPin, LOW);  delayMicroseconds(T1L);
  } else {
    digitalWrite(txPin, HIGH); delayMicroseconds(T0H);
    digitalWrite(txPin, LOW);  delayMicroseconds(T0L);
  }
}

void sendSync() {
  digitalWrite(txPin, HIGH); delayMicroseconds(TSH);
  digitalWrite(txPin, LOW);  delayMicroseconds(TSL);
}

