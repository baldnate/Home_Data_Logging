/* 
  temp.ino - firmware for weather shield node

  by Bald Nate

  Based off of Nathan Seidle's Weather Shield example firmware, which was based off of Mike Grusin's USB Weather Board code.

  This firmware collects data from a Sparkfun Weather Shield and sends it over serial.

  This code has no support for the following features of the weather shield:
    * GPS
    * wind
    * rain
    * light

  Hardware needed:
  Weather Shield - https://www.sparkfun.com/products/12081
  An Arduino (I used an Uno)

  Libraries needed:
  HTU21D - https://dlnmh9ip6v2uc.cloudfront.net/assets/9/f/8/8/5/5287be1e757b7f2f378b4567.zip
*/

#include <Wire.h>        // For general I2C
#include "HTU21D.h"      // For humidity sensor

/*******************
* Simple_MPL3115A2 *
*******************/
const int MPL3115A2_ADDRESS = 0x60;
const int OUT_P_MSB         = 0x01;
const int DR_STATUS         = 0x06;
const int PT_DATA_CFG       = 0x13;
const int CTRL_REG1         = 0x26;

const byte MPL_ARMED = 1;
const byte MPL_MEAS_READY = 2;

class Simple_MPL3115A2 {
  public:
    Simple_MPL3115A2() {}
    bool init(void) {
      IIC_Write(CTRL_REG1,   0x38); // barometer, cooked, 128 oversample, no reset, no OST, standby
      IIC_Write(PT_DATA_CFG, 0x07); // enable all the event bits (data ready, temp ready, pressure ready)
      IIC_Write(CTRL_REG1,   0x39); // barometer, cooked, 128 oversample, no reset, no OST, active
    }
    bool readTempAndPressure(float &rtemp, float &rpressure);

  private:
    bool waitForMeasurementComplete();
    byte IIC_Read(byte regAddr);
    void IIC_Write(byte regAddr, byte value);
};

// returns false if measurement failed
bool Simple_MPL3115A2::readTempAndPressure(float &rtemp, float &rpressure) {
  if (!waitForMeasurementComplete()) return false;

  // Read pressure and temp registers
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(OUT_P_MSB);  // Address of data to get
  Wire.endTransmission(false);
  Wire.requestFrom(MPL3115A2_ADDRESS, 5); // Request 5 bytes
  for (int waitCycles = 0; (Wire.available() < 5) && (waitCycles <= 10); ++waitCycles) delay(10);
  if (Wire.available() < 5) return false;

  byte pmsb, pcsb, plsb, tmsb, tlsb;
  pmsb = Wire.read();
  pcsb = Wire.read();
  plsb = Wire.read();
  tmsb = Wire.read();
  tlsb = Wire.read();

  // Pressure comes back as a left shifted 20 bit number
  long pressure_whole = (long) pmsb << 16 | (long) pcsb << 8 | (long) plsb;
  pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.
  plsb &= 0b00110000; //Bits 5/4 represent the fractional component
  plsb >>= 4; //Get it right aligned
  float pressure_decimal = (float) plsb / 4.0; //Turn it into fraction
  rpressure = (float) pressure_whole + pressure_decimal;

  // The least significant bytes l_altitude and l_temp are 4-bit,
  // fractional values, so you must cast the calulation in (float),
  // shift the value over 4 spots to the right and divide by 16 (since 
  // there are 16 values in 4-bits).
  bool negSign = false;
  if(tmsb > 0x7F) {
    word foo = 0;
    foo = ~((tmsb << 8) + tlsb) + 1;  //2’s complement
    tmsb = foo >> 8;
    tlsb = foo & 0x00F0; 
    negSign = true;
  }
  float templsb = (tlsb >> 4) / 16.0; //temp, fraction of a degree
  rtemp = (float)(tmsb + templsb);
  if (negSign) rtemp = -rtemp;
  return true;
}

// returns true if data has arrived, false if we timed out
bool Simple_MPL3115A2::waitForMeasurementComplete(void) {
  // spin until temp and pressure flags are both set
  byte ctrl = IIC_Read(DR_STATUS);
  int counter = 0;
  while (!(ctrl & 4) && !(ctrl & 2) && (counter <= 6)) {
    counter++;
    delay(100);
    ctrl = IIC_Read(DR_STATUS);
  }
  return counter <= 6;
}

byte Simple_MPL3115A2::IIC_Read(byte regAddr) {
  // This function reads one byte over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);
  byte result = Wire.endTransmission(false);

  Wire.requestFrom(MPL3115A2_ADDRESS, 1); // Request the data...

  int counter = 0;
  while (Wire.available() < 1) {
    delay(1);
    if (++counter == 100) break;
  }

  return Wire.read();
}

void Simple_MPL3115A2::IIC_Write(byte regAddr, byte value) {
  // This function writes one byto over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);
  Wire.write(value);
  byte result = Wire.endTransmission(true);
}
/**********************************************************
*                 End of Simple_MPL3115A2                 *
**********************************************************/

// TODO: I could remove the looping sum by keeping sum up-to-date all the time.
const byte AUTO_AVERAGER_SAMPLE_LENGTH = 15;
class AutoAverager {
  public:
    AutoAverager() {
      clear();
    }
    void clear(void) {
      index = 0;
      primed = false;
      for (byte i = 0; i < AUTO_AVERAGER_SAMPLE_LENGTH; ++i) samples[i] = 0.0;
    }
    float latch(float x) {
      if (!primed) {
        for (byte i = 0; i < AUTO_AVERAGER_SAMPLE_LENGTH; ++i) samples[i] = x;
        primed = true;
        return x;  
      } else {
        samples[index] = x;
        index = (index + 1) % AUTO_AVERAGER_SAMPLE_LENGTH;
        return getAverage();
      }
    }
    float getAverage(void) {
      float sum = 0.0;
      for (byte i = 0; i < AUTO_AVERAGER_SAMPLE_LENGTH; ++i) sum += samples[i];
      return sum / (AUTO_AVERAGER_SAMPLE_LENGTH * 1.0);
    }
  private:
    float samples[AUTO_AVERAGER_SAMPLE_LENGTH];
    byte index;
    bool primed;
};

// Pin constants note:
// Not all pin constants are referenced in this sketch.  They are included to document 
// what all pins could be in use on the Sparkfun Weather Shield.  Ex: GPS_TX

// Digital I/O pins
const byte RAIN       = 2;
const byte WSPEED     = 3;
const byte GPS_TX     = 4;
const byte GPS_RX     = 5;
const byte GPS_PWRCTL = 6; // Pulling this pin low puts GPS to sleep but maintains RTC and RAM
const byte LED_BLUE   = 7;
const byte LED_GREEN  = 8;

// Analog I/O pins
const byte WDIR    = A0;
const byte LIGHT   = A1;
const byte BATT    = A2;
const byte REF_3V3 = A3;

// Globals
unsigned long lastSecond;  // The millis counter to see when a second rolls by
Simple_MPL3115A2 myPressure;
HTU21D myHumidity;
AutoAverager runningPressure;
AutoAverager runningPressureTemp;
AutoAverager runningHumidity;
AutoAverager runningHumidityTemp;

float cToF(float c) {
  return (c * 9.0)/ 5.0 + 32.0;
}

bool isHumidityBogus(float x) {
  return x == 998.0 || x == 999.0;
}


#define DEFPACK(type) void pack(uint8_t *msg, type data, uint8_t *offset) { \
  *(type*) (msg + *offset) = data; \
  *offset = *offset + sizeof(type); \
}

DEFPACK(uint8_t);
DEFPACK(float);

bool getObservations(float &humidity, float &pressure, float &pTempF, float &hTempF) {
  humidity = myHumidity.readHumidity();
  if (isHumidityBogus(humidity)) {
    runningHumidity.clear();
    return false;
  } else {
    humidity = runningHumidity.latch(humidity);
  }

  float hTempC = myHumidity.readTemperature();
  hTempF = cToF(hTempC);
  if (isHumidityBogus(hTempF)) {
    runningHumidityTemp.clear();
    return false;
  } else {
    hTempF = runningHumidityTemp.latch(hTempF);
  }

  float pTempC    = -999.0;
  if (myPressure.readTempAndPressure(pTempC, pressure)) {
    pressure = runningPressure.latch(pressure);
    pTempF = runningPressureTemp.latch(cToF(pTempC));
  } else {
    myPressure.init();
    runningPressureTemp.clear();
    runningPressure.clear();
    return false;
  }

  return true;
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
  float humidity;
  float pressure;
  float pTempF;
  float hTempF;

  if (getObservations(humidity, pressure, pTempF, hTempF)) {
    pack(msg, magic, &offset);
    pack(msg, humidity, &offset);
    pack(msg, pressure, &offset);
    pack(msg, pTempF, &offset);
    pack(msg, hTempF, &offset);

    uint8_t checkSum = 0;
    for (uint8_t i = 0; i < offset; ++i) {
      Serial.write(msg[i]);
      checkSum += msg[i];
    }
    Serial.write(checkSum);
  }
}

void setup() {
  delay(1000); // Wait for a bit before proceeding...

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(LED_BLUE, HIGH);   // Both LEDs on == init in progress
  digitalWrite(LED_GREEN, HIGH);

  delay(200); // Leave the LEDs on for at least a bit...

  Serial.begin(9600);
  Wire.begin();
  myHumidity.begin();
  myPressure.init();
  
  pinMode(REF_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  digitalWrite(LED_BLUE, LOW); // both LEDs off == init (nearly) complete
  digitalWrite(LED_GREEN, LOW);

  lastSecond = millis();
}

void loop() {
  const unsigned int POLL_INTERVAL_IN_MS = 500;

  static bool ledState = false;
  static uint8_t messageBuffer[32];

  observeAndSend(messageBuffer);
  ledState = !ledState;
  digitalWrite(LED_GREEN, ledState ? LOW : HIGH);
  digitalWrite(LED_BLUE, ledState ? HIGH : LOW);
  delay(POLL_INTERVAL_IN_MS);
}

