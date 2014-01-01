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

const int tmp102Address = 0x48; // A0 pin tied to ground

float getTempF()
{
  int rawTemp = analogRead(0);
  float voltage = rawTemp * (5.0 / 1024.0);
  return (1.8 * ((voltage - 0.5) * 100.0)) + 32.0;
}

void setup()
{
//  Serial.begin(9600);
  
  pinMode(0, INPUT);

  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	     // Bits per sec
  vw_set_tx_pin(4);

//  Serial.println("tmp36 online");
}

void loop()
{
  float fahrenheit = getTempF();
//  Serial.print("tmp36F:");
//  Serial.println(fahrenheit);

  // TODO: Create RF message format, do that instead of both the serial prints and the RF test data.
  char *msg = "tmp36-09876543210987654321\n";
  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx(); // Wait until the whole message is gone

  delay(500);
}
