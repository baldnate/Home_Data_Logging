#stty -F /dev/ttyUSB0 cs8 115200 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts

import serial
ser = serial.Serial('/dev/tty.usbserial-A602ZBVU', 115200)

while True:
	print ser.readline()