/* 
  wx.ino - firmware for wireless weather shield node

  by Bald Nate

  Based off of Nathan Seidle's Weather Shield example firmware, which was based off of Mike Grusin's USB Weather Board code.

  This firmware collects data from a Sparkfun Weather Shield and sends it over an RF Transmitter.  It does not support the GPS
  portion of the shield.

  Hardware needed:
  Weather Shield - https://www.sparkfun.com/products/12081
  434MHz RF Transmitter - https://www.sparkfun.com/products/10534
  An Arduino (I used an Uno)

  Libraries needed:
  VirtualWire - http://www.airspayce.com/mikem/arduino/VirtualWire/
  HTU21D - https://dlnmh9ip6v2uc.cloudfront.net/assets/9/f/8/8/5/5287be1e757b7f2f378b4567.zip
*/

#include <Wire.h>        // For general I2C
#include "HTU21D.h"      // For humidity sensor
#include <VirtualWire.h> // For RF transmitter

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
const byte RF_PTT     = 9; // RF push-to-talk pin (i.e.: data out)

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

const byte DEBOUNCE_DURATION_IN_MS = 10;
void debounce(volatile unsigned long &count, volatile unsigned long &lastTime) {
  unsigned long currentTime = millis();
  if (currentTime - lastTime > DEBOUNCE_DURATION_IN_MS) { // ignore switch-bounces for 10ms
    count++; // 
    lastTime = currentTime;
  }
}

volatile unsigned long lastRainIRQ = 0, rainClicks = 0;
void rainIRQ() {
  debounce(rainClicks, lastRainIRQ);
}

volatile unsigned long lastWindIRQ = 0, windClicks = 0;
void wspeedIRQ() {
  debounce(windClicks, lastWindIRQ);
}

void setup() {
  delay(1000); // Wait for a bit before proceeding...

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(LED_BLUE, HIGH);   // Both LEDs on == init in progress
  digitalWrite(LED_GREEN, HIGH);

  delay(200); // Leave the LEDs on for at least a bit...

  Wire.begin();
  myHumidity.begin();
  myPressure.init();

  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	           // Bits per sec
  vw_set_tx_pin(RF_PTT);
  
  pinMode(WSPEED, INPUT_PULLUP);
  pinMode(RAIN, INPUT_PULLUP);
  pinMode(REF_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  digitalWrite(LED_BLUE, LOW); // both LEDs off == init (nearly) complete
  digitalWrite(LED_GREEN, LOW);

  lastSecond = millis();

  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);
  interrupts();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSecond >= 900) {
    lastSecond = now;
    sendObservations();
  }
  delay(random(200));
}

//Read the wind direction sensor, return heading in degrees
unsigned int getWindDirection() {
  unsigned int adc = analogRead(WDIR); // get the current reading from the sensor

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 380) return (113);
  if (adc < 393) return (68);
  if (adc < 414) return (90);
  if (adc < 456) return (158);
  if (adc < 508) return (135);
  if (adc < 551) return (203);
  if (adc < 615) return (180);
  if (adc < 680) return (23);
  if (adc < 746) return (45);
  if (adc < 801) return (248);
  if (adc < 833) return (225);
  if (adc < 878) return (338);
  if (adc < 913) return (0);
  if (adc < 940) return (293);
  if (adc < 967) return (315);
  if (adc < 990) return (270);
  return (-1); // error, disconnected?
}

// Returns the voltage of the light sensor based on the 3.3V rail
// This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float getLightLevel()
{
  float operatingVoltage = analogRead(REF_3V3);
  float lightSensor = analogRead(LIGHT);
  operatingVoltage = 3.3 / operatingVoltage; // The reference voltage is 3.3V
  lightSensor = operatingVoltage * lightSensor;
  return lightSensor;
}

float cToF(float c) {
  return (c * 9.0)/ 5.0 + 32.0;
}

bool isHumidityBogus(float x) {
  return x == 998.0 || x == 999.0;
}

bool observeConditions(uint8_t msg[32]) {
  /*  Message format as follows.
      Header: 
        magic(byte): 0xCC
        sender(byte): 0x01 (for the wx node)
      Data
        rainticks(uint): Running count of rain cup tips.
        humidity(float) : Percent humidity.
        pressure(float): Pressure in Pascals.
        pTempf(float) : Temperature in degF, as read from pressure sensor.
        windticks(ulong): Running count of wind ticks.
        winddir(float): Wind direction in degrees azimuth.
        hTempf(float) : Temperature in degF, as read from humidity sensor.
        light(float) : Light level.

      Total packet size: 32 bytes */

  // Just blue on == measurements being collected
  digitalWrite(LED_BLUE, HIGH);

  float humidity  = myHumidity.readHumidity();
  if (isHumidityBogus(humidity)) {
    runningHumidity.clear();
    return false;
  } else {
    humidity = runningHumidity.latch(humidity);
  }

  float hTempC = myHumidity.readTemperature();
  float hTempF = cToF(hTempC);
  if (isHumidityBogus(hTempF)) {
    runningHumidityTemp.clear();
    return false;
  } else {
    hTempF = runningHumidityTemp.latch(hTempF);
  }

  float pTempC    = -999.0;
  float pTempF    = -999.0;
  float pressure  = -999.0;
  if (myPressure.readTempAndPressure(pTempC, pressure)) {
    pressure = runningPressure.latch(pressure);
    pTempF = runningPressureTemp.latch(cToF(pTempC));
  } else {
    myPressure.init();
    runningPressureTemp.clear();
    runningPressure.clear();
    return false;
  }

  float lightLevel = getLightLevel();

  msg[0] = 0xCC;
  msg[1] = 0x01;
  *(unsigned int*) (msg +  2) = rainClicks;
  *(float*)(msg +  4) = humidity;
  *(float*)(msg +  8) = pressure;
  *(float*)(msg + 12) = pTempF;
  *(unsigned long*)(msg + 16) = windClicks;
  *(float*)(msg + 20) = getWindDirection() * 1.0;
  *(float*)(msg + 24) = hTempF;
  *(float*)(msg + 28) = lightLevel;
  
  digitalWrite(LED_BLUE, LOW);

  return true;
}

void sendObservations() {
  const byte msgLength = 32;
  uint8_t msg[msgLength];

  if (observeConditions(msg)) {
    // just green on == transmission in progress
    digitalWrite(LED_GREEN, HIGH);
    vw_send(msg, msgLength);
    vw_wait_tx();
    digitalWrite(LED_GREEN, LOW);
  }
}
