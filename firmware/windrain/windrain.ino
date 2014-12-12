/* 
  windrain.ino - firmware for wireless weather shield node

  by Bald Nate

  Portions based on code from Nathan Seidle's Weather Shield example firmware.

  This firmware collects wind speed, wind direction, and rainfall data and returns it via serial.

  Target Arduino:
  Arduino Pro Mini 328 - 5V/16MHz

  Additional hardware:
  Weather meters (https://www.sparkfun.com/products/8942)
*/

// Pins
const byte RAIN       =  2;
const byte WSPEED     =  3;
const byte LED        = 13;
const byte WDIR       = A0;

// Constants
const byte DEBOUNCE_DURATION_IN_MS = 10;

inline void debounce(volatile unsigned long &count, volatile unsigned long &lastTime) {
  unsigned long currentTime = millis();
  if (currentTime - lastTime > DEBOUNCE_DURATION_IN_MS) {
    count++;
    lastTime = currentTime; // TODO: should probably be outside the conditional (debounce instead of throttle)
  }
}

volatile unsigned long lastRainIRQ = 0, interruptRainClicks = 0;
void rainIRQ() {
  debounce(interruptRainClicks, lastRainIRQ);
}

volatile unsigned long lastWindIRQ = 0, interruptWindClicks = 0;
void wspeedIRQ() {
  debounce(interruptWindClicks, lastWindIRQ);
}

void getWindAndRain(unsigned long &windClicks, unsigned long &rainClicks) {
  noInterrupts();
  windClicks = interruptWindClicks;
  rainClicks = interruptRainClicks;
  interrupts();
}

// For more info on the wind vane hardware, see the datasheet here:
// https://www.sparkfun.com/datasheets/Sensors/Weather/Weather%20Sensor%20Assembly..pdf
// I stripped out the intermediates for a few reasons:
// 1. I wasn't able to get my vane to ever close on two resistors at once.
// 2. I didn't feel like calculating another 8 ADC code points.
// 3. This gets rid of the half-degree directions.
uint8_t getWindDirection() {
  unsigned int adc = analogRead(WDIR); // get the current reading from the sensor

  if (adc < 190)  return 2; //   1.0 K  178 - 180
  if (adc < 330)  return 3; //   2.2 K  325 - 327
  if (adc < 470)  return 4; //   3.9 K  463 - 465
  if (adc < 660)  return 1; //   8.2 K  650 - 653
  if (adc < 800)  return 5; //  16.0 K  790 - 792
  if (adc < 910)  return 0; //  33.0 K  895 - 900
  if (adc < 960)  return 7; //  64.9 K  954 - 956
  if (adc < 1000) return 6; // 120.0 K  982 - 989
  return 255;               // error, disconnected?
}

#define DEFPACK(type) void pack(uint8_t *msg, type data, uint8_t *offset) { \
  *(type*) (msg + *offset) = data; \
  *offset = *offset + sizeof(type); \
}

DEFPACK(uint8_t);
DEFPACK(unsigned long);

void getObservations(unsigned long &windClicks, unsigned long &rainClicks, uint8_t &windDir) {
  getWindAndRain(windClicks, rainClicks);
  windDir = getWindDirection();
}

void observeAndSend(uint8_t msg[32]) {
  /* 
    Message format as follows.
      Header: 
        magic(uint8_t)
      Data:
        rainClicks(unsigned long): Running count of rain cup tips.
        windClicks(unsigned long): Running count of wind ticks.
        windDir(uint8_t): Wind direction in degrees azimuth / 45.
      Footer:
        checkSum(uint8_t)
  */

  uint8_t offset = 0;
  uint8_t magic = 0xAA;
  unsigned long windClicks;
  unsigned long rainClicks;
  uint8_t windDir;

  getObservations(windClicks, rainClicks, windDir);

  pack(msg, magic, &offset);
  pack(msg, rainClicks, &offset);
  pack(msg, windClicks, &offset);
  pack(msg, windDir, &offset);

  uint8_t checkSum = 0;
  for (uint8_t i = 0; i < offset; ++i) {
    Serial.write(msg[i]);
    checkSum += msg[i];
  }
  Serial.write(checkSum);
}

void setup() {
  pinMode(WSPEED, INPUT_PULLUP);
  pinMode(RAIN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  Serial.begin(9600);

  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);
  interrupts();
}

void loop() {
  const unsigned int POLL_INTERVAL_IN_MS = 500;

  static bool ledState = false;
  static uint8_t messageBuffer[32];

  observeAndSend(messageBuffer);
  ledState = !ledState;
  digitalWrite(LED, ledState ? LOW : HIGH);
  delay(POLL_INTERVAL_IN_MS);
}
