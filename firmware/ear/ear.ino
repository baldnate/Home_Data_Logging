/*
  ear.ino - firmware for wireless listener and temp node

  by Bald Nate

  tmp102 code based on info from http://bildr.org/2011/01/tmp102-arduino/

  Hardware needed:
  434MHz RF Receiver - https://www.sparkfun.com/products/10532
  TMP102 - https://www.sparkfun.com/products/11931
  An Arduino (I used an Uno)

  Libraries needed:
  VirtualWire - http://www.airspayce.com/mikem/arduino/VirtualWire/
*/

/*
  TODOS
  -----
  * Both the RF protocol and the serial protocol need defining.
*/


#include <VirtualWire.h>
#include <Wire.h>

const int tmp102Address = 0x48; // A0 pin tied to ground

float getTempF()
{
  Wire.requestFrom(tmp102Address,2); 
  byte MSB = Wire.read();
  byte LSB = Wire.read();
  int TemperatureSum = ((MSB << 8) | LSB) >> 4;
  return (0.1125 * TemperatureSum) + 32;
}

long lastPost;  // milli counter of last time we posted an update

void setup()
{
  // Wait for a bit before proceeding...
  delay(2000);

  Serial.begin(115200);
  Serial.println("EAR INIT START");

  Wire.begin();
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	           // Bits per sec
  vw_set_rx_pin(2);
  vw_rx_start();             // Start the receiver PLL running

  lastPost = millis();

  Serial.println("EAR INIT COMPLETE");
}

void emitEarTempF()
{
  float fahrenheit = getTempF();
  Serial.println("BEGIN");
  Serial.println("NODE:EAR");
  Serial.print("TEMPF:");
  Serial.println(fahrenheit);
  Serial.println("END");
}

void loop()
{
  bool posted = false;
  long iterTime = millis();

  uint8_t msg[VW_MAX_MESSAGE_LEN];
  uint8_t msglen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(msg, &msglen)) // Non-blocking
  {
    /*
      wx message format
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

    if (msg[0] == 0xcc && msg[1] == 0x01)
    {
      Serial.println("BEGIN");
      Serial.println("NODE:WX");
      Serial.print("RAINTICKS:");
      Serial.println(*(unsigned int*) (msg +  2));
      Serial.print("HUMIDITY:");
      Serial.println(*(float*)(msg +  4));
      Serial.print("PRESSURE:");
      Serial.println(*(float*)(msg +  8));
      Serial.print("TEMPF:");
      Serial.println(*(float*)(msg + 12));
      Serial.print("WINDSPEED:");
      Serial.println(*(float*)(msg + 16));
      Serial.print("WINDDIR:");
      Serial.println(*(float*)(msg + 20));
      Serial.println("END");
    }

    posted = true;
  }

  if (posted || (iterTime > lastPost + 30000))
  {
    emitEarTempF();
    posted = true;
  }

  if (posted)
  {
    Serial.print("uptime:");
    Serial.println(iterTime/1000);
    lastPost = iterTime;
  }
}
