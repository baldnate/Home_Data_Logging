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
from ez_tweet import EZTweet
from wx_pws import WundergroundPWS

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
		if not samples:
			self.pwsspeed = 0
			self.speed = 0.0
			self.pwsdir = self.dir = None
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
			self.pwsspeed = int(round(self.speed))
			self.pwsdir = None if self.dir is None else int(round(self.dir))


	def returnGreater(self, x):
		if x.speed > self.speed:
			return x
		if x.speed == self.speed and x.time > self.time:
			return x
		return self
	
	def isCalm(self):
		# According to section 2-6-5 of http://www.faa.gov/air_traffic/publications/atpubs/atc/atc0206.html
		return self.pwsspeed < 3

	def tweet(self, showDir = True):
		if self.isCalm():
			return "calm"
		elif self.dir == None or not showDir:
			return "{0:.0f}mph".format(round(self.speed))
		else:
			return "{1} @ {0:.0f}mph".format(round(self.speed), wx_math.degreesToCompass(round(self.dir)))


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
		self.dewpointf = 0 			   # dewpoint in degF
		self.heatindexf = 0
		self.windchillf = 0

		self.lastUpdate = datetime.datetime.utcnow()

	def updatePWS(self, pws):
		pws.update(
			action = 'updateraw',
			ID = 'XXXXX',
			PASSWORD = 'XXXXX',
			softwaretype = 'baldwx',

			dateutc = self.lastUpdate.strftime("%Y-%m-%d %H:%M:%S"),
			tempf = '%.1f' % self.tempf,
			indoortempf = '%.1f' % self.indoortempf,
			windspeedmph = self.windCurr.pwsspeed,
			winddir = self.windCurr.pwsdir,
			windgustmph = self.gustCurr.pwsspeed,
			windgustdir = self.gustCurr.pwsdir,
			windspdmph_avg2m  = self.windAvg2m.pwsspeed,
			winddir_avg2m = self.windAvg2m.pwsdir,
			windgustmph_10m = self.windGust10m.pwsspeed,
			windgustdir_10m = self.windGust10m.pwsdir,
			humidity = int(round(self.humidity)),
			dewptf = '%.1f' % self.dewpointf,
			rainin = '%.3f' % self.rainin,
			dailyrainin = '%.3f' % self.dailyrainin,
			baromin = '%.2f' % self.baromin
		)

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
		self.windchillf = wx_math.windChill(self.tempf, self.windAvg2m.speed)
		self.baromin = wx_math.pascalsToAltSettingInHg(observation["pressure"], prefs["WX_ALTITUDE_IN_METERS"])
		if self.baromin is None:
			print "Bogus pressure encountered!  pascals:{0}, alt:{1}".format(observation["pressure"], prefs["WX_ALTITUDE_IN_METERS"])
		self.indoortempf = observation["indoortempf"]
		self.lastUpdate = observation["timestamp"]

	def formatApparentTemperature(self):
		if self.windchillf is not None:
			wc = round(self.windchillf)
			if wc < round(self.tempf) - 1:
				return " (WC {0:.0f}°F)".format(wc)
		else:
			hi = round(self.heatindexf)
			if hi > round(self.tempf) + 1:
				return " (HI {0:.0f}°F)".format(hi)
		return None

	def console(self):
		retVal = self.tweet()
		retVal.append("{0:.0f}% RH".format(round(self.humidity)))
		retVal.append("{0:.0f}°F DP".format(round(self.dewpointf)))
		if self.baromin is None:
			retVal.append(None)
		else:
			retVal.append("{0:.2f}\"Hg".format(self.baromin))
		retVal.append("{0:.0f}°F indoor".format(round(self.indoortempf)))
		retVal.append("wCur {0}".format(self.windCurr.tweet()))
		retVal.append("gCur {0}".format(self.gustCurr.tweet()))
		retVal.append("g10m {0}".format(self.windGust10m.tweet()))
		return retVal

	def formatWindGust(self):
		wind = self.windAvg2m
		gust = self.windGustTweet
		if wind.isCalm() and gust.speed < 5:
			return "calm"
		else:
			if round(gust.speed) > round(wind.speed):
				return "wind {0} (gust {1})".format(self.windAvg2m.tweet(), self.windGustTweet.tweet(False))
			else:
				return "wind {0}".format(self.windAvg2m.tweet())

	def formatRain(self, tag, value):
		if value < 0.01:
			return None
		return "rain({0}) {1:.2f}\"".format(tag, value)

	def tweet(self):
		retVal = []
		retVal.append("{0:.0f}°F".format(round(self.tempf)))
		retVal.append(self.formatApparentTemperature())
		retVal.append(self.formatWindGust())
		retVal.append(self.formatRain("hour", self.rainin))
		retVal.append(self.formatRain("today", self.dailyrainin))
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
tweetRetryDelay = 0
lastTweetText = ""
updates = 0
secrets = json.load(open('secrets.json'))

twitter = EZTweet(secrets['APP_KEY'], secrets['APP_SECRET'], secrets['OAUTH_TOKEN'], secrets['OAUTH_TOKEN_SECRET'])
pws = WundergroundPWS()

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
		print "\t".join([(x if x is not None else "XXXXX") for x in wud.console()])
		#wud.updatePWS(pws)
		lastConsoleTime = time
	if tweetInterval and (time - lastTweetTime).total_seconds() >= tweetInterval + tweetRetryDelay:
		status = ", ".join([x for x in wud.tweet() if x is not None])
		print "Tweeting: %s" % status
		retryTime = twitter.tweet(status)
		if retryTime == -1:
			lastTweetTime = time
			tweetRetryDelay = 0
		else:
			print "Tweet failed.  Next attempt in %i seconds" % retryTime
			tweetRetryDelay += retryTime
