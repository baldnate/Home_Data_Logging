/* 
  serialmaster.ino - firmware for serial aggregation node

  by Bald Nate

  Summary:
  This firmware listens to the two sensing nodes via software timed serial
  and relays them back to a host via hardware timed serial.

  Target Arduino:
  Uno

  Dependencies:
  SoftwareSerial
*/

#define READ_BUFFER_SIZE 1000

#include "SoftwareSerial.h"

// Pins
// Hardware serial: 0 and 1
const byte WINDRAINNODE_RX  = 2;
const byte WINDRAINNODE_TX  = 3;
const byte TEMPNODE_RX  = 4;
const byte TEMPNODE_TX  = 5;

// Constants
const unsigned long LISTEN_TIME_IN_MS = 400;

// Globals
SoftwareSerial tempPort(TEMPNODE_RX, TEMPNODE_TX);
SoftwareSerial windRainPort(WINDRAINNODE_RX, WINDRAINNODE_TX);
uint8_t readBuffer[READ_BUFFER_SIZE] = {0};

void setup() {
  delay(500);
  windRainPort.begin(9600);
  tempPort.begin(9600);
  Serial.begin(9600);
}

void readBytes(SoftwareSerial &port, uint8_t *buffer)
{
  unsigned long entryTime = millis();
  long bytesRead = 0;
  buffer[0] = NULL;
  while (millis() - entryTime < LISTEN_TIME_IN_MS) {
    while (port.available()) {
      bytesRead++;
      *buffer = port.read();
      ++buffer;
    }
  }
}

#define DEFUNPACK(type) void unpack(uint8_t *msg, type *dataOut, uint8_t *offset) { \
  *dataOut = *(type*) (msg + *offset); \
  *offset = *offset + sizeof(type); \
}

DEFUNPACK(uint8_t);
DEFUNPACK(float);
DEFUNPACK(unsigned long);

uint8_t *findMagic(uint8_t *buffer)
{
  if (buffer[0] == NULL) return NULL;
  for (int i = 0; i < READ_BUFFER_SIZE; ++i) {
    if (buffer[i] == 0xAA) {
      return buffer + i + 1;
    }
  }
  return NULL;
}

void processTempNode(uint8_t readBuffer[READ_BUFFER_SIZE])
{
  uint8_t *msg = findMagic(readBuffer);
  if (msg != NULL) {
    uint8_t offset = 0;
    float humidity, pressure, pTempF, hTempF;
    uint8_t checkSum;
    unpack(msg, &humidity, &offset);
    unpack(msg, &pressure, &offset);
    unpack(msg, &pTempF, &offset);
    unpack(msg, &hTempF, &offset);
    unpack(msg, &checkSum, &offset);

    if (calculateCheckSum(msg, offset) == checkSum) {
      Serial.print("{\"name\": \"temp\"");
      Serial.print(", \"humidity\": ");
      Serial.print(humidity);
      Serial.print(", \"pressure\": ");
      Serial.print(pressure);
      Serial.print(", \"pTempf\": ");
      Serial.print(pTempF);
      Serial.print(", \"hTempf\": ");
      Serial.print(hTempF);
      Serial.println("}");
    }
  }
}

uint8_t calculateCheckSum(uint8_t msg[READ_BUFFER_SIZE], uint8_t unpackOffset) {
  uint8_t checkSum = 0xAA;
  unpackOffset--;
  while (unpackOffset > 0) {
    unpackOffset--;
    checkSum += msg[unpackOffset];
  }
  return checkSum;
}

void processWindRainNode(uint8_t readBuffer[READ_BUFFER_SIZE])
{
  uint8_t *msg = findMagic(readBuffer);
  if (msg != NULL) {
    uint8_t offset = 0;
    unsigned long rainClicks, windClicks;
    uint8_t windDir;
    uint8_t checkSum;
    unpack(msg, &rainClicks, &offset);
    unpack(msg, &windClicks, &offset);
    unpack(msg, &windDir, &offset);
    unpack(msg, &checkSum, &offset);

    if (calculateCheckSum(msg, offset) == checkSum) {
      Serial.print("{\"name\": \"windrain\", ");
      Serial.print("\"rainticks\": ");
      Serial.print(rainClicks);
      Serial.print(", \"windticks\": ");
      Serial.print(windClicks);
      Serial.print(", \"winddir\": ");
      Serial.print(windDir * 45.0);
      Serial.println("}");
    }
  }
}

void loop() {
  readBytes(tempPort, readBuffer);
  windRainPort.listen();
  processTempNode(readBuffer);

  readBytes(windRainPort, readBuffer);
  tempPort.listen();
  processWindRainNode(readBuffer);
}