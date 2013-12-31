/*
  wx.ino - firmware for wireless weather shield node

  by Bald Nate

  Based off of Nathan Seidle's Weather Shield example firmware, which was based off of Mike Grusin's USB Weather Board code.

  This firmware collects data from a Sparkfun Weather Shield and sends it over an RF Transmitter.  It does not support the GPS
  portion of the shield.

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
  return (1.8 * TemperatureSum*0.0625) + 32;
}

long lastPost;  // milli counter of last time we posted an update

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	           // Bits per sec
  vw_set_rx_pin(2);
  vw_rx_start();             // Start the receiver PLL running

  lastPost = millis();

  Serial.println("ear online");
}

void loop()
{
  bool posted = false;
  long iterTime = millis();

  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    int i;
    Serial.print("Got: ");
    for (i = 0; i < buflen; i++)
    {
      Serial.print(buf[i]);
      Serial.print(" ");
    }
    Serial.println("");
    posted = true;
  }

  if (posted || (iterTime > lastPost + 1000))
  {
    float fahrenheit = getTempF();
    Serial.print("earF:");
    Serial.println(fahrenheit);
    posted = true;
  }

  if (posted)
  {
    lastPost = iterTime;
  }
}
