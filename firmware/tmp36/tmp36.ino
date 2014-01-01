/*
  tmp36.ino - firmware for wireless listener and temp node

  by Bald Nate

  Hardware needed:
  434MHz RF Receiver - https://www.sparkfun.com/products/10532
  TMP36 - https://www.sparkfun.com/products/10988
  An Arduino (I used an old Diecimila)

  Libraries needed:
  VirtualWire - http://www.airspayce.com/mikem/arduino/VirtualWire/
*/

/*
  TODOS
  -----
  * Both the RF protocol and the serial protocol need defining.
*/

#include <VirtualWire.h>

float getTempF()
{
  int rawTemp = analogRead(0);
  float voltage = rawTemp * (5.0 / 1024.0);
  return (1.8 * ((voltage - 0.5) * 100.0)) + 32.0;
}

void setup()
{
  pinMode(0, INPUT);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);	     // Bits per sec
  vw_set_tx_pin(4);
}

void loop()
{
  float fahrenheit = getTempF();

  // TODO: Create RF message format, do that instead of both the serial prints and the RF test data.
  char *msg = "tmp36-09876543210987654321\n";
  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx(); // Wait until the whole message is gone

  delay(500);
}
