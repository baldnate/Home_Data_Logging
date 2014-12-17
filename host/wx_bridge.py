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
import simplejson as json
import datetime
import math
import wx_math
from ez_tweet import EZTweet
from wx_pws import WundergroundPWS
from copy import copy


def timeWindow(samples, time):
    """
    return culled list of just what has happened in the last <time> seconds
    """

    if not samples:
        return []

    if len(samples) == 1:
        return samples

    now = samples[0].time
    i = 1
    while i < len(samples) and (now - samples[i].time).total_seconds() < time:
        i += 1

    # at this point, i is pointing just after the last item in the window.
    if i >= len(samples):
        return samples
    else:
        retVal = samples[0:i - 1]
        return retVal


def formatTemp(tempF, tag=""):
    return "%s %.1f°F" % (tag, tempF)


def tickDelta(tickbig, ticksmall, maxticks):
    if tickbig >= ticksmall:
        return tickbig - ticksmall
    else:
        return (maxticks - ticksmall + 1) + tickbig


class RawRainSample(object):

    def __init__(self, ticks, time):
        self.ticks = ticks
        self.time = time


class RainData(object):

    def __init__(self, samples=[]):
        self.samples = samples
        self.MAXTICKS = 65535

    def push(self, sample):
        self.samples.insert(0, sample)

    def timeWindow(self, seconds):
        return RainData(timeWindow(self.samples, seconds))

    def rainfall(self):
        if len(self.samples) > 1:
            return tickDelta(self.samples[0].ticks, self.samples[-1].ticks, self.MAXTICKS) * 0.011
        else:
            return 0


class RawWindSample(object):

    def __init__(self, dir, ticks, time):
        super(RawWindSample, self).__init__()
        self.dir = dir
        self.ticks = ticks
        self.time = time


class WindSpeed(object):

    def __init__(self, samples=[]):
        MAXTICKS = 4294967295
        if not samples:
            self.pwsspeed = 0
            self.speed = 0.0
            self.pwsdir = self.dir = None
            self.time = datetime.datetime.utcnow()
        else:
            deltaTime = samples[0].time - samples[-1].time
            deltaTicks = tickDelta(samples[0].ticks, samples[-1].ticks, MAXTICKS)
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

    def tweet(self, showDir=True):
        if self.isCalm():
            return "calm"
        elif self.dir is None or not showDir:
            return "{0:.0f}mph".format(round(self.speed))
        else:
            return "{1} @ {0:.0f}mph".format(round(self.speed), wx_math.degreesToCompass(round(self.dir)))


class WindData(object):

    def __init__(self, samples=[]):
        self.samples = samples

    def push(self, sample):
        self.samples.insert(0, sample)

    def timeWindow(self, seconds):
        return WindData(timeWindow(self.samples, seconds))

    def avg(self):
        if self.samples and len(self.samples) > 1:
            return WindSpeed(self.samples)
        else:
            return WindSpeed()

    def gust(self):
        max = WindSpeed()
        if self.samples:
            newerSample = self.samples[0]
            for olderSample in self.samples[1:]:
                max = max.returnGreater(WindSpeed([newerSample, olderSample]))
                newerSample = olderSample
        return max


class WeatherUndergroundData(object):

    def __init__(self, pwsInterval, tweetInterval):
        self.maxInterval = max(pwsInterval, tweetInterval, 120, 600)
        self.tweetInterval = tweetInterval if tweetInterval else self.maxInterval
        self.currInterval = pwsInterval if pwsInterval else self.maxInterval
        self.windData = WindData()       # raw wind samples
        self.windCurr = WindSpeed()      # instant velocity (wunderground winddir & windspeedmph)
        self.gustCurr = WindSpeed()      # 30 sec gust (wunderground windgustmph & windgustdir)
        self.windAvg2m = WindSpeed()     # 2 min avg (wunderground windspdmph_avg2m & winddir_avg2m)
        self.windGust10m = WindSpeed()   # 10 min gust (wunderground windgustmph_10m & windgustdir_10m)
        self.windGustTweet = WindSpeed()  # tweet interval gust (baldwx)
        self.rainData = RainData()
        self.humidity = 0              # humidity in percent
        self.tempf = 0                 # outdoor temp in degF
        self.rainin = 0                # accumulated rainfall in the last 60 min
        self.dailyrainin = 0           # rain inches so far today (local time)
        self.baromin = 0               # barometric pressure inches (altimeter setting)
        self.indoortempf = 0           # indoor temp in degF
        self.dewpointf = 0             # dewpoint in degF
        self.heatindexf = 0
        self.windchillf = 0

        self.lastUpdate = datetime.datetime.utcnow()

    def updatePWS(self, pws):
        pws.update(
            dateutc=self.lastUpdate.strftime("%Y-%m-%d %H:%M:%S"),
            tempf='%.1f' % self.tempf,
            indoortempf='%.1f' % self.indoortempf,
            windspeedmph=self.windCurr.pwsspeed,
            winddir=self.windCurr.pwsdir,
            windspdmph_avg2m=self.windAvg2m.pwsspeed,
            winddir_avg2m=self.windAvg2m.pwsdir,
            humidity=int(round(self.humidity)),
            dewptf='%.1f' % self.dewpointf,
            rainin='%.3f' % self.rainin,
            dailyrainin='%.3f' % self.dailyrainin,
            baromin='%.2f' % self.baromin
        )
            # windgustmph=self.gustCurr.pwsspeed,
            # windgustdir=self.gustCurr.pwsdir,
            # windgustmph_10m=self.windGust10m.pwsspeed,
            # windgustdir_10m=self.windGust10m.pwsdir,

    def pushRain(self, observation):
        now = observation["timestamp"]
        self.rainData.push(RawRainSample(observation["rainticks"], now))

        localnow = now.replace(tzinfo=timezone.utc).astimezone(tz=None)
        localmidnight = datetime.combine(localnow.date(), time(0))
        delta = localnow - localmidnight

        data1h = self.rainData.timeWindow(60 * 60)
        data1d = self.rainData.timeWindow(delta.total_seconds())

        self.rainin = data1h.rainfall()
        self.dailyrainin = data1d.rainfall()

    def pushWind(self, observation):
        now = observation["timestamp"]
        self.windData.push(RawWindSample(observation["winddir"], observation["windticks"], now))

        # slice for various time windows
        dataTweet = self.windData.timeWindow(self.tweetInterval)
        dataCurr = self.windData.timeWindow(self.currInterval)
        data10m = self.windData.timeWindow(10 * 60)
        data2m = data10m.timeWindow(2 * 60)

        self.windCurr = dataCurr.avg()
        self.gustCurr = dataCurr.gust()
        self.windAvg2m = data2m.avg()
        self.windGust10m = data10m.gust()
        self.windGustTweet = dataTweet.gust()

        self.windData = self.windData.timeWindow(self.maxInterval)  # discard data older than we care about

    def pushObservation(self, observation):
        if observation["name"] == "windrain":
            self.pushWind(observation)
            self.pushRain(observation)
        if observation["name"] == "temp":
            self.humidity = observation["humidity"]
            self.ptempf = wx_math.fixBogusTempReading(observation["pTempf"])
            self.htempf = wx_math.fixBogusTempReading(observation["hTempf"])
            self.tempf = (self.ptempf + self.htempf) / 2.0
            self.dewpointf = wx_math.dewpoint(self.tempf, self.humidity)
            self.heatindexf = wx_math.temperatureHumidityIndex(self.tempf, self.humidity)
            self.windchillf = wx_math.windChill(self.tempf, self.windAvg2m.speed)
            self.baromin = wx_math.pascalsToAltSettingInHg(observation["pressure"], prefs["WX_ALTITUDE_IN_METERS"])
            if self.baromin is None:
                print "Bogus pressure encountered!  pascals:{0}, alt:{1}".format(observation["pressure"], prefs["WX_ALTITUDE_IN_METERS"])
        self.lastUpdate = observation["timestamp"]

    def formatApparentTemperature(self):
        if self.windchillf is not None:
            wc = round(self.windchillf)
            if wc < round(self.tempf) - 1:
                return formatTemp(wc, "wind chill")
        elif self.heatindexf is not None:
            hi = round(self.heatindexf)
            if hi > round(self.tempf) + 1:
                return formatTemp(hi, "heat index")
        return None

    def console(self):
        retVal = self.tweet()
        retVal.append("{0:.0f}% RH".format(round(self.humidity)))
        retVal.append(formatTemp(self.dewpointf, "DP"))
        if self.baromin is None:
            retVal.append(None)
        else:
            retVal.append("{0:.2f}\"Hg".format(self.baromin))
        retVal.append(formatTemp(self.indoortempf, "indoor"))
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
        retVal.append(formatTemp(self.tempf))
        retVal.append(self.formatApparentTemperature())
        retVal.append(self.formatWindGust())
        retVal.append(self.formatRain("hour", self.rainin))
        retVal.append(self.formatRain("today", self.dailyrainin))
        return retVal


def getChunk(ser):
    while True:
        line = "".join(ser.readline().split(chr(0)))
        yield line


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description='wx_bridge')
    parser.add_argument('-t', '--doctest', help='Run doctests', required=False, action="store_true")
    parser.add_argument('-d', '--debug', help='Run in debug mode', required=False, action="store_true")
    args = parser.parse_args()

    if args.doctest:
        import doctest
        doctest.testmod()
        exit()

    debugMode = args.debug

    prefs = json.load(open('prefs.json'))

    ser = None
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

    reportKey = "REPORT_CFG"
    if debugMode:
        reportKey = "REPORT_CFG_DEBUG"

    tweetInterval = prefs[reportKey]["tweet"]
    consoleInterval = prefs[reportKey]["console"]
    pwsInterval = prefs[reportKey]["pws"]
    prefill = prefs[reportKey]["prefill"]
    pws = prefs[reportKey]["pws"]
    wud = WeatherUndergroundData(pwsInterval, tweetInterval)

    # assume the worst: that we just updated before this script ran
    lastPWSTime = lastTweetTime = lastConsoleTime = lastUpdateRateTime = datetime.datetime.utcnow()
    tweetRetryDelay = 0
    lastTweetText = ""
    updates = 0
    secrets = json.load(open('secrets.json'))

    twitter = EZTweet(secrets['APP_KEY'], secrets['APP_SECRET'], secrets['OAUTH_TOKEN'], secrets['OAUTH_TOKEN_SECRET'])
    pws = WundergroundPWS(secrets['PWS_ID'], secrets['PWS_PASSWORD'], rtfreq=pwsInterval)

    print "wx_bridge initialized and listening to {0}".format(serialPort)
    if debugMode:
        print "debugging mode ON"

    for line in getChunk(ser):
        time = datetime.datetime.utcnow()
        try:
            data = json.loads(line)
        except ValueError as e:
            print "Ignoring malformed packet:"
            print line
            print "Raw dump:"
            ords = []
            for x in line:
                ords.append(str(ord(x)))
            print ",".join(ords)
            print "Exception:"
            print e
            continue
        try:
            data['timestamp'] = time
            wud.pushObservation(data)
            updates = updates + 1
            if prefill:
                prefill -= 1
                continue
            if updates % 100 == 0:
                print "{0:.2f} reports/sec".format(updates / (time - lastUpdateRateTime).total_seconds())
                updates = 0
                lastUpdateRateTime = time
            if pwsInterval and (time - lastPWSTime).total_seconds() >= pwsInterval:
                wud.updatePWS(pws)
                lastPWSTime = time
            if consoleInterval and (time - lastConsoleTime).total_seconds() >= consoleInterval:
                print " ".join([(x if x is not None else "XXXXX") for x in wud.console()])
                lastConsoleTime = time
            if tweetInterval and ((time - lastTweetTime).total_seconds() >= (tweetInterval + tweetRetryDelay)):
                status = ", ".join([x for x in wud.tweet() if x is not None])
                retryTime = twitter.tweet(status)
                if retryTime == -1:
                    lastTweetTime = time
                    tweetRetryDelay = 0
                else:
                    print "Tweet failed.  Next attempt in %i seconds" % retryTime
                    tweetRetryDelay += retryTime
        except Exception as e:
            print "Unexpected exception caught!"
            print "Line being processed:\n{0}\n".format(line)
            print "Exception details:"
            print e