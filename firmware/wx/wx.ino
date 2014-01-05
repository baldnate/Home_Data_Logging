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
  HTU21D and MPL3115A2 - https://dlnmh9ip6v2uc.cloudfront.net/assets/9/f/8/8/5/5287be1e757b7f2f378b4567.zip
*/

/*
  TODOS
  -----
  * Barometer needs work
  * A lot of the wind code looks suspicious.  I am probably going to move most of it out of the firmware.
  * I've tried to eliminate all the dead code in here, but I know there is still some left.
  * The rain code looks a little dodgy as well.  Same as wind: a lot of it will move out of the firmware.
  * ... and some style work, just enough to get it to where it doesn't surprise me after putting it down for a few months.
  * oh, and the data should actually be sent through the RF link rather than serial.  And the protocol needs to be defined.
*/

#include <Wire.h>        // For general I2C
#include "MPL3115A2.h"   // For pressure sensor
#include "HTU21D.h"      // For humidity sensor
#include <VirtualWire.h> // For RF transmitter

// Pin constants notes:
// * I include pin constants I don't use so as to document what pins are in use
//   (or could be in use) on the Sparkfun Weather Shield.
// * Sorted by pin number.

// Digital I/O pins

const byte RAIN       = 2;
const byte WSPEED     = 3;
const byte GPS_TX     = 4;
const byte GPS_RX     = 5;
const byte GPS_PWRCTL = 6; // Pulling this pin low puts GPS to sleep but maintains RTC and RAM
const byte STAT1      = 7; // Status LED
const byte STAT2      = 8; // Status LED
const byte RF_PTT     = 9; // RF push-to-talk pin (i.e.: data out)

// Analog I/O pins
const byte WDIR    = A0;
const byte LIGHT   = A1;
const byte BATT    = A2;
const byte REF_3V3 = A3;

// Globals
long lastSecond;  // The millis counter to see when a second rolls by
long lastWindCheck = 0;
MPL3115A2 myPressure;
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

  pinMode(STAT1, OUTPUT); //Status LED Blue
  pinMode(STAT2, OUTPUT); //Status LED Green

  digitalWrite(STAT1, HIGH);
  digitalWrite(STAT2, HIGH);

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
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 

  //Configure the humidity sensor
  myHumidity.begin();

  digitalWrite(STAT1, LOW);
  digitalWrite(STAT2, LOW);

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
    digitalWrite(STAT1, HIGH); //Blink stat LED
    
    lastSecond += 1000;

    sendObservations();

    digitalWrite(STAT1, LOW); //Turn off stat LED
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

void sendObservations()
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

  const byte msgLength = 24;
  uint8_t msg[msgLength];

  msg[0] = 0xcc;
  msg[1] = 0x01;
  *(unsigned int*) (msg +  2) = rainTicks;
  *(float*)(msg +  4) = myHumidity.readHumidity();
  *(float*)(msg +  8) = myPressure.readPressure();
  *(float*)(msg + 12) = myPressure.readTempF();;
  *(float*)(msg + 16) = getWindSpeed();
  *(float*)(msg + 20) = getWindDirection() * 1.0;

  digitalWrite(STAT2, HIGH);
  vw_send(msg, msgLength);
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(STAT2, LOW);
}


