# encoding: utf-8

# wx_bridge.py
#
# By baldnate
#
# Glue code between observation nodes and outbound reporting.
#
# Current sources:
# * ear.ino (json over serial)
# 
# Current destinations:
# * Twitter (@baldwx)
# * weatherunderground PWS

# TODOS
# * rain
# * conversion from pressure in absolute pascals to altimeter setting inches
# * ticks lists compression via eliding points without data
# * ticks lists should also keep around the oldest point in some form or fashion
# * posting to twitter or wunderground

import serial
import json
import datetime

class RawRainSample(object):
	def __init__(self, ticks, time):
		self.ticks = ticks
		self.time = time

class RainData(object):
	def __init__(self, samples=[]):
		self.samples = samples

	def push(self, sample):
		self.samples.insert(0, sample)

	def timeWindow(self, seconds):
		# return culled list of just what has happened in the last <seconds> seconds
		if len(self.samples) > 0:
			now = self.samples[0].time
			return RainData([x for x in self.samples if (now - x.time).total_seconds() <= seconds])
		else:
			return RainData()

	def rainfall(self):
		if len(self.samples) > 1:
			return (self.samples[0].ticks - self.samples[-1].ticks) * 0.011
		else:
			return 0


class RawWindSample(object):
	def __init__(self, dir, ticks, time):
		super(RawWindSample, self).__init__()
		self.dir = dir
		self.ticks = ticks
		self.time = time


class WindSpeed(object):
	def __init__(self, newerSample=None, olderSample=None):
		if (newerSample and olderSample):
			deltaTime = newerSample.time - olderSample.time
			deltaTicks = newerSample.ticks - olderSample.ticks
			if deltaTime:
				self.speed = deltaTicks / deltaTime.total_seconds() * 1.492
			else:
				self.speed = 0.0
			self.dir = newerSample.dir
			self.time = newerSample.time
		else:
			self.speed = 0.0
			self.dir = None
			self.time = datetime.datetime.utcnow()

	def returnGreater(self, x):
		if x.speed > self.speed:
			return x
		if x.speed == self.speed and x.time > self.time:
			return x
		return self

	def format(self):
		if self.dir == None:
			return ""
		return "{0:.0f} MPH @ {1:.0f}°".format(round(self.speed), round(self.dir))


class WindData(object):
	def __init__(self, samples=[]):
		self.samples = samples

	def push(self, sample):
		self.samples.insert(0, sample)

	def timeWindow(self, seconds):
		# return culled list of just what has happened in the last <seconds> seconds
		if len(self.samples) > 0:
			now = self.samples[0].time
			return WindData([x for x in self.samples if (now - x.time).total_seconds() <= seconds])
		else:
			return WindData()

	def curr(self):
		if len(self.samples) > 1:
			return WindSpeed(self.samples[0], self.samples[1])
		else:
			return WindSpeed()

	def avg(self):
		if len(self.samples) > 1:
			return WindSpeed(self.samples[0], self.samples[-1])
		else:
			return WindSpeed()

	def gust(self):
		max = WindSpeed()
		if len(self.samples) > 0:
			newerSample = self.samples[0]
			for olderSample in self.samples[1:]:
				max = max.returnGreater(WindSpeed(newerSample, olderSample))
				newerSample = olderSample
		return max


class WeatherUndergroundData(object):
	def __init__(self):
		super(WeatherUndergroundData, self).__init__()
		self.windData = WindData()     # raw wind samples
		self.windCurr = WindSpeed()    # instant velocity (wunderground winddir & windspeedmph)
		self.gustCurr = WindSpeed()    # 30 sec gust (wunderground windgustmph & windgustdir)
		self.windAvg2m = WindSpeed()   # 2 min avg (wunderground windspdmph_avg2m & winddir_avg2m)
		self.windGust10m = WindSpeed() # 10 min gust (wunderground windgustmph_10m & windgustdir_10m)
		self.windAvg15m = WindSpeed()  # 15 min avg (baldwx)
		self.windGust15m = WindSpeed() # 15 min gust (baldwx)
		self.humidity = 0              # humidity in percent
		self.tempf = 0				   # outdoor temp in degF
		self.rainin = 0      		   # accumulated rainfall in the last 60 min
		self.dailyrainin = 0           # rain inches so far today (local time)
		self.baromin = 0               # barometric pressure inches (altimeter setting)
		self.indoortempf = 0           # indoor temp in degF
		self.lastUpdate = datetime.datetime.utcnow()

	def pushRain(self, observation):
		now = observation["timestamp"]
		self.rainData.push(RawRainSample(observation["rainticks"], now))

		localnow = now.replace(tzinfo=timezone.utc).astimezone(tz=None)
		localmidnight = datetime.combine(localnow.date(), time(0))
		delta = localnow - localmidnight

		data1h = self.rainData.timeWindow(60*60)
		data1d = self.rainData.timeWindow(delta.total_seconds())

		self.rainin = data1h.rainfall()
		self.dailyrainin = data1d.rainfall()

	def pushWind(self, observation):
		now = observation["timestamp"]
		self.windData.push(RawWindSample(observation["winddir"], observation["windticks"], now))

		# slice for various time windows
		data15m = self.windData.timeWindow(15*60)
		data10m = data15m.timeWindow(10*60)
		data2m = data10m.timeWindow(2*60)
		data30s = data2m.timeWindow(30)

		self.windCurr = data30s.curr()
		self.gustCurr = data30s.gust()
		self.windAvg2m = data2m.avg()
		self.windGust10m = data10m.gust()
		self.windAvg15m = data15m.avg()
		self.windGust15m = data15m.gust()

		self.windData = data15m # discard data older that 15 minutes

	def pushObservation(self, observation):
		self.pushWind(observation)
		self.humidity = observation["humidity"]
		self.tempf = observation["tempf"]
		self.baromin = observation["pressure"]     # TODO
		self.indoortempf = observation["indoortempf"]
		self.lastUpdate = observation["timestamp"]

	def format(self):
		retVal = ""
		retVal += "dateutc:      {0}\n".format(self.lastUpdate.isoformat(' '))
		retVal += "wind:         {0}\n".format(self.windCurr.format())
		retVal += "gust:         {0}\n".format(self.gustCurr.format())
		retVal += "2m wind avg:  {0}\n".format(self.windAvg2m.format())
		retVal += "10m gust:     {0}\n".format(self.windGust10m.format())
		retVal += "15m wind avg: {0}\n".format(self.windAvg15m.format())
		retVal += "15m gust:     {0}\n".format(self.windGust15m.format())
		retVal += "humidity:     {0:.0f}%\n".format(round(self.humidity))
		retVal += "temp:         {0:.0f} °F\n".format(round(self.tempf))
		retVal += "hourly rain:  {0:.2f} in\n".format(self.rainin)
		retVal += "daily rain:   {0:.2f} in\n".format(self.dailyrainin)
		retVal += "pressure:     {0:.1f} Pa\n".format(self.baromin)
		retVal += "indoor temp:  {0:.0f} °F\n".format(round(self.indoortempf))
		return retVal

ser = None

try:
	ser = serial.Serial('/dev/tty.usbserial-A602ZBVU', 115200)
except:
	ser = serial.Serial('/dev/ttyUSB0', 115200)

wud = WeatherUndergroundData()
lastTime = datetime.datetime.utcnow()

while True:
	line = ser.readline()
	time = datetime.datetime.utcnow()
	try:
		data = json.loads(line)
	except ValueError, e:
		continue
	data['timestamp'] = time
	wud.pushObservation(data)
	if (time - lastTime).total_seconds() >= 1:
		print wud.format()
		lastTime = time

