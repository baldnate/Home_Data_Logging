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
# * stdout

import serial
import json
import datetime
import math
import wx_math
from twython import Twython

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
	def __init__(self, samples=None):
		if samples == None:
			self.speed = 0.0
			self.dir = None
			self.time = datetime.datetime.utcnow()
		else:
			deltaTime = samples[0].time - samples[-1].time
			deltaTicks = samples[0].ticks - samples[-1].ticks
			if deltaTime:
				self.speed = deltaTicks / deltaTime.total_seconds() * 1.492
			else:
				self.speed = 0.0
			(angle, magnitude) = wx_math.angularMean([x.dir for x in samples if x.dir != 65535])
			if magnitude > .4:
				self.dir = angle
			else:
				self.dir = None
			self.time = samples[0].time


	def returnGreater(self, x):
		if x.speed > self.speed:
			return x
		if x.speed == self.speed and x.time > self.time:
			return x
		return self
	
	def isCalm(self):
		return round(self.speed) == 0

	def tweet(self):
		if self.isCalm():
			return "calm"
		elif self.dir == None:
			return "{0:.0f}mph".format(round(self.speed))
		else:
			return "{1}{0:.0f}mph".format(round(self.speed), wx_math.degreesToArrow(round(self.dir)))


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

	def avg(self):
		if len(self.samples) > 1:
			return WindSpeed(self.samples)
		else:
			return WindSpeed()

	def gust(self):
		max = WindSpeed()
		if len(self.samples) > 0:
			newerSample = self.samples[0]
			for olderSample in self.samples[1:]:
				max = max.returnGreater(WindSpeed([newerSample, olderSample]))
				newerSample = olderSample
		max.dir = None
		return max


class WeatherUndergroundData(object):
	def __init__(self, currInterval, tweetInterval):
		self.currInterval = currInterval
		self.maxInterval = max(currInterval, tweetInterval, 120, 600)
		if tweetInterval:
			self.tweetInterval = tweetInterval
		else:
			self.tweetInterval = self.maxInterval
		self.windData = WindData()       # raw wind samples
		self.windCurr = WindSpeed()      # instant velocity (wunderground winddir & windspeedmph)
		self.gustCurr = WindSpeed()      # 30 sec gust (wunderground windgustmph & windgustdir)
		self.windAvg2m = WindSpeed()     # 2 min avg (wunderground windspdmph_avg2m & winddir_avg2m)
		self.windGust10m = WindSpeed()   # 10 min gust (wunderground windgustmph_10m & windgustdir_10m)
		self.windGustTweet = WindSpeed() # tweet interval gust (baldwx)
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
		dataTweet = self.windData.timeWindow(self.tweetInterval)
		dataCurr = self.windData.timeWindow(self.currInterval)
		data10m = self.windData.timeWindow(10*60)
		data2m = data10m.timeWindow(2*60)

		self.windCurr = dataCurr.avg()
		self.gustCurr = dataCurr.gust()
		self.windAvg2m = data2m.avg()
		self.windGust10m = data10m.gust()
		self.windGustTweet = dataTweet.gust()

		self.windData = self.windData.timeWindow(self.maxInterval) # discard data older than we care about

	def pushObservation(self, observation):
		self.pushWind(observation)
		self.humidity = observation["humidity"]
		self.tempf = observation["tempf"]
		self.dewpointf = wx_math.dewpoint(self.tempf, self.humidity)
		self.heatindexf = wx_math.temperatureHumidityIndex(self.tempf, self.humidity)
		self.windchillf = wx_math.windChill(self.tempf, self.windCurr.speed)
		self.baromin = wx_math.pascalsToAltSettingInHg(observation["pressure"], prefs["WX_ALTITUDE_IN_METERS"])
		self.indoortempf = observation["indoortempf"]
		self.lastUpdate = observation["timestamp"]

	def formatApparentTemperature(self):
		if self.windchillf is not None:
			wc = round(self.windchillf)
			if wc > round(self.tempf) - 1:
				return " (feels like {0:.0f}°F".format(wc)
		else:
			hi = round(self.heatindexf)
			if hi > round(self.tempf) + 1:
				return " (feels like {0:.0f}°F".format(hi)
		return ""

	def format(self):
		retVal = ""
		retVal += "dateutc:      {0}\n".format(self.lastUpdate.isoformat(' '))
		retVal += "wind:         {0}\n".format(self.windCurr.format())
		retVal += "gust:         {0}\n".format(self.gustCurr.format())
		retVal += "2m wind avg:  {0}\n".format(self.windAvg2m.format())
		retVal += "10m gust:     {0}\n".format(self.windGust10m.format())
		retVal += "humidity:     {0:.0f}%\n".format(round(self.humidity))
		retVal += "temp:         {0:.0f}°F\n".format(round(self.tempf))
		retVal += "hourly rain:  {0:.2f}\"\n".format(self.rainin)
		retVal += "daily rain:   {0:.2f}\"\n".format(self.dailyrainin)
		retVal += "pressure:     {0:.1f} Pa\n".format(self.baromin)
		retVal += "indoor temp:  {0:.0f}°F\n".format(round(self.indoortempf))
		return retVal

	def tweet(self):
		retVal = ""
		retVal += "{0:.0f}°F".format(round(self.tempf))
		retVal += self.formatApparentTemperature()
		retVal += ", {0:.0f}%RH".format(round(self.humidity))
		retVal += ", {0:.0f}dew°F".format(round(self.dewpointf))
		retVal += ", {0:.2f}\"Hg".format(self.baromin)
		if self.windCurr.isCalm():
			retVal += ", calm"
		else:
			retVal += ", wind {0}".format(self.windCurr.tweet())
		if self.windGustTweet.speed > self.windCurr.speed:
			retVal += " (gust {0})".format(self.windGustTweet.tweet())
		if self.rainin > 0.0:
			retVal += ", rain(hour) {0:.2f}\"".format(self.rainin)
		if self.dailyrainin > 0.0:
			retVal += ", rain(today) {0:.2f}\"".format(self.dailyrainin)
		return retVal


ser = None

prefs = json.load(open('prefs.json'))
debugMode = prefs["DEBUG"] != 0

connected = False
for serialPort in prefs["SERIAL_PORTS"]:
	try:
		ser = serial.Serial(serialPort, 115200)
		connected = True
		break
	except:
		continue

if not(connected):
	print "Could not connect to serial port, check connections and prefs.json[SERIAL_PORTS]."
	exit(-1)

reportKey = "REPORT_CFG"
if debugMode:
	reportKey = "REPORT_CFG_DEBUG"

tweetInterval = prefs[reportKey]["tweet"]
consoleInterval = prefs[reportKey]["console"]
prefill = prefs[reportKey]["prefill"]
wud = WeatherUndergroundData(prefs[reportKey]["curr"], tweetInterval)

lastTweetTime = lastConsoleTime = datetime.datetime.utcnow() - datetime.timedelta(1) # one day ago, to trigger an immediate update
lastUpdateRateTime = datetime.datetime.utcnow()
lastTweetText = ""
updates = 0
secrets = json.load(open('secrets.json'))

print "wx_bridge initialized and listening to {0}".format(serialPort)

while True:
	line = ser.readline()
	time = datetime.datetime.utcnow()
	try:
		data = json.loads(line)
	except ValueError, e:
		print "Malformed packet received, ignoring."
		continue
	data['timestamp'] = time
	wud.pushObservation(data)
	updates = updates + 1
	if prefill:
		prefill -= 1
		continue
	if updates % 20 == 0:
		print "{0:.2f} updates/sec".format(updates/(time - lastUpdateRateTime).total_seconds())	
		updates = 0
		lastUpdateRateTime = time
	if (time - lastConsoleTime).total_seconds() >= consoleInterval:
		print wud.tweet()
		lastConsoleTime = time
	if tweetInterval and (time - lastTweetTime).total_seconds() >= tweetInterval:
		status = wud.tweet()
		if lastTweetText != status:
			print "Tweeting..."
			try:
				twitter = Twython(secrets['APP_KEY'], secrets['APP_SECRET'], secrets['OAUTH_TOKEN'], secrets['OAUTH_TOKEN_SECRET'])
				twitter.update_status(status=status)
				print "Tweet successful"
			except twython.exceptions.TwythonError as e:
				print "Got exception:\n{0}".format(str(e))
				# Twitter API returned a 503 (Service Unavailable), Over capacity
				# ^^^ retry
			lastTweetTime = time
		else:
			print "Not tweeting due to unchanged conditions."
