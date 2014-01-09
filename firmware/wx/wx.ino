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

/*
  TODOS
  -----
  * Barometer conversion needs work
  * A lot of the wind code looks suspicious.  I am probably going to move most of it out of the firmware.
  * I've tried to eliminate all the dead code in here, but I know there is still some left.
  * The rain code looks a little dodgy as well.  Same as wind: a lot of it will move out of the firmware.
  * ... and some style work, just enough to get it to where it doesn't surprise me after putting it down for a few months.
  * Humidity set resolution?
  * Pull Simple_MPL3115A2 out into a library?
  * Simple_MPL needs refactoring... it moved from being standby OST to auto-measuring with polling for DR.  I could
    simplify it even more with that in mind.
  * ... but at least it ran overnight with losing its mind, like the old code would.
*/

#include <Wire.h>        // For general I2C
#include "HTU21D.h"      // For humidity sensor
#include <VirtualWire.h> // For RF transmitter

/***********************************************************
*                     Simple_MPL3115A2                     *
************************************************************
* Simplified and fixed version of MPL3115A2 from SparkFun. *
* - Simplified by assuming one mode of operation.          *
* - Fixed by making it more power and bus friendly.        *
***********************************************************/
const int MPL3115A2_ADDRESS = 0x60;
const int OUT_P_MSB = 0x01;
const int CTRL_REG1 = 0x26;

const byte MPL_ARMED = 1;
const byte MPL_MEAS_READY = 2;

class Simple_MPL3115A2 {
  public:
    Simple_MPL3115A2() {}
    bool begin(void) {
      Wire.begin();
      IIC_Write(CTRL_REG1, 0x38); // barometer, cooked, 128 oversample, no reset, no OST, in standby
      IIC_Write(0x13, 0x07); // enable events
      IIC_Write(CTRL_REG1, 0x39); // barometer, cooked, 128 oversample, no reset, no OST, active
      arm();
    }
    bool readTempAndPressure(float &rtemp, float &rpressure);

  private:
    byte state;
    void arm();
    void reset();
    bool waitForMeasurementComplete();
    byte IIC_Read(byte regAddr);
    void IIC_Write(byte regAddr, byte value);
};

// returns false if measurement failed
bool Simple_MPL3115A2::readTempAndPressure(float &rtemp, float &rpressure) {
  byte rearmAttempts = 0;
  bool abort = false;

  while (!abort) {
    switch (state) 
    {
      case MPL_ARMED:
        if (waitForMeasurementComplete()) {
          state = MPL_MEAS_READY;
        } else {
          if (rearmAttempts < 10) {
            arm();
            rearmAttempts++;
          } else {
            abort = true;
          }
        }
        break;

      case MPL_MEAS_READY:
        // Read pressure and temp registers
        Wire.beginTransmission(MPL3115A2_ADDRESS);
        Wire.write(OUT_P_MSB);  // Address of data to get
        Wire.endTransmission(false);
        Wire.requestFrom(MPL3115A2_ADDRESS, 5); // Request 5 bytes
        for (int waitCycles = 0; (Wire.available() < 5) && (waitCycles <= 10); ++waitCycles) {
          Serial.print("m");
          delay(1);
        }
        if (Wire.available() < 5) {
          arm();
          rearmAttempts++;
          break;
        }

        byte pmsb, pcsb, plsb, tmsb, tlsb;
        pmsb = Wire.read();
        pcsb = Wire.read();
        plsb = Wire.read();
        tmsb = Wire.read();
        tlsb = Wire.read();

        arm(); // immediately re-arm
        state = MPL_ARMED;

        // Pressure comes back as a left shifted 20 bit number
        long pressure_whole = (long)pmsb<<16 | (long)pcsb<<8 | (long)plsb;
        pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.
        plsb &= 0b00110000; //Bits 5/4 represent the fractional component
        plsb >>= 4; //Get it right aligned
        float pressure_decimal = (float)plsb/4.0; //Turn it into fraction
        rpressure = (float)pressure_whole + pressure_decimal;

        // The least significant bytes l_altitude and l_temp are 4-bit,
        // fractional values, so you must cast the calulation in (float),
        // shift the value over 4 spots to the right and divide by 16 (since 
        // there are 16 values in 4-bits). 
        float templsb = (tlsb>>4)/16.0; //temp, fraction of a degree
        rtemp = (float)(tmsb + templsb);
        return true;
    }
  }

  Serial.print("f");
  Serial.print(state);
  rtemp = 0.0;
  rpressure = 0.0;
  reset();
  return false;
}

void Simple_MPL3115A2::arm(void) {
//  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings to be safe
//  tempSetting |= (1<<1); //Set OST bit
//  IIC_Write(CTRL_REG1, tempSetting);
//  IIC_Write(CTRL_REG1, 0x3A); // barometer, cooked, 128 oversample, no reset, OST, in standby
  state = MPL_ARMED;
}

void Simple_MPL3115A2::reset(void) {
//  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings to be safe
//  tempSetting |= (1<<1); //Set OST bit
//  IIC_Write(CTRL_REG1, tempSetting);
  IIC_Write(CTRL_REG1, 0x04); // reset
  byte ctrl = IIC_Read(CTRL_REG1);
  int counter = 0;
  while ((ctrl & 4) && (counter <= 20)) {
    counter++;
    delay(100);
    ctrl = IIC_Read(CTRL_REG1);
  }
  arm();
}

// returns true if data has arrived, false if we timed out
bool Simple_MPL3115A2::waitForMeasurementComplete(void) {
  // spin until OST is cleared or we get tired of waiting
  byte ctrl = IIC_Read(0x06);
  int counter = 0;
  while (!(ctrl & 4) && !(ctrl & 2) && (counter <= 6)) {
    counter++;
    delay(550);
    ctrl = IIC_Read(0x06);
  }
  return counter <= 6;
}

byte Simple_MPL3115A2::IIC_Read(byte regAddr)
{
  Serial.print("R:");
  Serial.print(regAddr, HEX);

  // This function reads one byte over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);  // Address of CTRL_REG1
  byte result = Wire.endTransmission(false);
  if (result)
  {
    Serial.print(" error ");
    Serial.print(result);
  }

  Wire.requestFrom(MPL3115A2_ADDRESS, 1); // Request the data...

  int counter = 0;
  while (Wire.available() < 1)
  {
    delay(1);
    if (++counter == 100)
    {
      break;
    }
  }

  byte retVal = Wire.read();
  Serial.print("=");
  Serial.print(retVal, HEX);
  Serial.print(" ");
  return retVal;
}

void Simple_MPL3115A2::IIC_Write(byte regAddr, byte value)
{
  Serial.print("W:");
  Serial.print(regAddr, HEX);
  Serial.print(",");
  Serial.print(value, HEX);
  Serial.print(" ");

  // This function writes one byto over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);
  Wire.write(value);
  byte result = Wire.endTransmission(true);
  if (result)
  {
    Serial.print("!");
    Serial.print(result);
    Serial.print("!");
  }
}
/**********************************************************
*                 End of Simple_MPL3115A2                 *
**********************************************************/


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
long lastSecond;  // The millis counter to see when a second rolls by
long lastWindCheck = 0;
Simple_MPL3115A2 myPressure;
HTU21D myHumidity;


volatile unsigned long rainLast, rainTicks;
void rainIRQ()
{
  unsigned long currentTime = millis();

  // ignore switch-bounce glitches less than 10mS after initial edge
  if (currentTime - rainLast > 10)
  {
    rainTicks++;
    rainLast = currentTime;
  }
}


volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;
void wspeedIRQ()
{
  unsigned long currentTime = millis();

  // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
  if (currentTime - lastWindIRQ > 10)
  {
    windClicks++; // There is 1.492MPH for each click per second.
    lastWindIRQ = currentTime;
  }
}


void setup()
{
  // Wait for a bit before proceeding...
  delay(2000);

  Serial.begin(115200);

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // Both LEDs on == init in progress
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);

  // Initialise the IO and ISR
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	     // Bits per sec
  vw_set_tx_pin(RF_PTT);
  
  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP);   // input from wind meters rain gauge sensor
  
  pinMode(REF_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  //Configure the pressure sensor
  myPressure.begin(); // Get sensor online

  //Configure the humidity sensor
  myHumidity.begin();

  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, LOW);

  lastSecond = millis();

  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();
}

void loop()
{
  if(millis() - lastSecond >= 1000)
  {
    lastSecond += 1000;
    sendObservations();
  }

  delay(100);
}


// Returns the instantaneous wind speed
float getWindSpeed()
{
  unsigned long currentTime = millis();
  float deltaTime = currentTime - lastWindCheck; // 750ms

  deltaTime /= 1000.0; // Covert to seconds

  float windSpeed = (float)windClicks / deltaTime; // 3 / 0.750s = 4

  windClicks = 0; // Reset and start watching for new wind
  lastWindCheck = currentTime;

  windSpeed *= 1.492; // 4 * 1.492 = 5.968MPH

  return(windSpeed);
}

//Read the wind direction sensor, return heading in degrees
unsigned int getWindDirection() 
{
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

float cToF(float c)
{
  return (c * 9.0)/ 5.0 + 32.0;
}

bool isHumidityBogus(float x)
{
  return x == 998.0 || x == 999.0 ;
}

bool observeConditions(uint8_t msg[24])
{
  /*
    Message format as follows.
    Header: 
      magic(byte): 0xCC
      sender(byte): 0x01 (for the wx node)
    Data
      rainticks(uint): Running count of rain cup tips.
      humidity(float) : Percent humidity.
      pressure(float): Pressure in Pascals.
      tempf(float) : Temperature in degF.
      windspeed(float): Wind speed in MPH.
      winddir(float): Wind direction in degrees azimuth.

    Total packet size: 24 bytes
  */

  bool success = true;

  digitalWrite(LED_BLUE, HIGH);

  float tempC = 0.0;
  float pressure = 0.0;
  if (myPressure.readTempAndPressure(tempC, pressure))
  {
    float tempF = cToF(tempC);
    Serial.print("pressure: ");
    Serial.print(pressure);
    Serial.print("\ttempF: ");
    Serial.println(tempF);
  }
  else
  {
    Serial.println("pressure and temp acq failed\t");
  }

  float humidity = myHumidity.readHumidity();
  Serial.print("\thumidity: ");
  Serial.println(humidity);


  msg[0] = 0xcc;
  msg[1] = 0x01;
  *(unsigned int*) (msg +  2) = rainTicks;
  *(float*)(msg +  4) = humidity;
  *(float*)(msg +  8) = pressure;
  *(float*)(msg + 12) = cToF(tempC);
  *(float*)(msg + 16) = getWindSpeed();
  *(float*)(msg + 20) = getWindDirection() * 1.0;
  digitalWrite(LED_BLUE, LOW);

  return success;
}

void sendObservations()
{
  const byte msgLength = 24;
  uint8_t msg[msgLength];

  if (observeConditions(msg))
  {
    digitalWrite(LED_GREEN, HIGH);
    vw_send(msg, msgLength);
    vw_wait_tx();
    digitalWrite(LED_GREEN, LOW);
  }
}


