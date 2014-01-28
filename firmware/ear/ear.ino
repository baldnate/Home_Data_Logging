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

void setup()
{
  // Wait for a bit before proceeding...
  delay(2000);

  Serial.begin(115200);
  Wire.begin();
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	           // Bits per sec
  vw_set_rx_pin(2);
  vw_rx_start();             // Start the receiver PLL running
}

void loop()
{
  uint8_t msg[VW_MAX_MESSAGE_LEN];
  uint8_t msglen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(msg, &msglen)) // Non-blocking
  {
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


    if (msg[0] == 0xcc && msg[1] == 0x01)
    {
      Serial.print("{\"name\": \"wx\", ");
      Serial.print("\"rainticks\": ");
      Serial.print(*(unsigned int*) (msg +  2));
      Serial.print(", \"humidity\": ");
      Serial.print(*(float*)(msg +  4));
      Serial.print(", \"pressure\": ");
      Serial.print(*(float*)(msg +  8));
      Serial.print(", \"pTempf\": ");
      Serial.print(*(float*)(msg + 12));
      Serial.print(", \"windticks\": ");
      Serial.print(*(unsigned long*)(msg + 16));
      Serial.print(", \"winddir\": ");
      Serial.print(*(float*)(msg + 20));
      Serial.print(", \"hTempf\": ");
      Serial.print(*(float*)(msg + 24));
      Serial.print(", \"light\": ");
      Serial.print(*(float*)(msg + 28));
      Serial.print(", \"indoortempf\": ");
      Serial.print(getTempF());
      Serial.println("}");
    }
  }
}
