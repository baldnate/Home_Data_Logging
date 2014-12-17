# wx_bridge.py - by baldnate
# Simple test script for troubleshooting issues with ear.ino

import serial
import json

prefs = json.load(open('prefs.json'))

connected = False
for serialPort in prefs["SERIAL_PORTS"]:
    try:
        ser = serial.Serial(serialPort, 9600)
        connected = True
        break
    except:
        continue

if not(connected):
    print "Could not connect to serial port, check connections and prefs.json[SERIAL_PORTS]."
    exit(-1)

while True:
    print ser.readline()
