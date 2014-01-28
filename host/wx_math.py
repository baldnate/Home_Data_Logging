# encoding: utf-8

# wx_math.py - dumping ground for a bunch of weather related math
#
# By baldnate with credit noted in individual functions

import math

def fuzzyEqual(a, b):
    """
    Fuzzy equality for floats.
    >>> fuzzyEqual(0, 0)
    True
    >>> fuzzyEqual(2, 2.001)
    True
    >>> fuzzyEqual(2, 2.5)
    False
    """
    return abs(a-b) < .01

def fixBogusTempReading(bogusF):
    """
    Fix for a two's complement error on the weather station firmware.
    >>> fuzzyEqual(fixBogusTempReading(491.23), 30.43)
    True
    >>> fixBogusTempReading(0)
    0
    """
    if bogusF > 250:
        return cToF(-1 * (128 - (fToC(bogusF) % 128)))
    else:
        return bogusF

def cToF(degC):
    """
    Convert temperature from °C to °F
    >>> cToF(100)
    212.0
    >>> cToF(0)
    32.0
    """
    return (degC * 9.0) / 5.0 + 32.0


def fToC(degF):
    """
    Convert temperature from °F to °C
    >>> fToC(212)
    100.0
    >>> fToC(32)
    0.0
    """
    return (degF - 32.0) * 5.0 / 9.0


def pascalsToMb(pascals):
    """
    Converts pascals to millibar
    >>> pascalsToMb(1)
    0.01
    """
    return pascals / 100.0


def mbToInchesHg(mb):
    """
    Converts millibar to inches of Hg (http://www.lamons.com/public/pdf/engineering/PressureConversionFormulas.pdf)
    >>> round(mbToInchesHg(203))
    6.0
    """
    return 0.02953 * mb


def pascalsToAltSettingInHg(pascals, altitudeInMeters):
    """
    Calculate quation for absolute pressure to altimeter setting pressure transcribed from
    http://www.srh.noaa.gov/images/epz/wxcalc/altimeterSetting.pdf
    >>> round(pascalsToAltSettingInHg(102700, 100) * 100) / 100
    30.68
    >>> round(pascalsToAltSettingInHg(99577.93, 269.933) * 100) / 100
    30.35
    >>> pascalsToAltSettingInHg(0, 269.933) is None
    True
    """
    a = pascalsToMb(pascals) - 0.3  # Pmb - 0.3
    h = altitudeInMeters
    if a < 0.0:
        return None
    i0 = math.pow(a, 0.190284)
    i1 = ((h / i0) * 0.000084228806861) + 1
    i2 = math.pow(i1, 5.255302600323727)
    i3 = i2 * a
    return mbToInchesHg(i3)


def dewpoint(degF, rh):
    """
    Calculate dewpoint from dry bulb temp and relative humidity.
    Equation transcribed from http://ag.arizona.edu/azmet/dewpoint.html
    >>> round(dewpoint(100, 50))
    78.0
    """
    tempC = fToC(degF)
    b = (math.log(rh / 100.0) + ((17.27 * tempC) / (237.3 + tempC))) / 17.27
    return cToF((237.3 * b) / (1.0 - b))


def windChill(degF, windMPH):
    """
    Calculate windchill from temperature and windspeed.
    Equation and test points from http://www.nws.noaa.gov/om/windchill/
    >>> round(windChill(0, 35))
    -27.0
    >>> round(windChill(40, 5))
    36.0
    >>> round(windChill(-45, 60))
    -98.0
    >>> windChill(60,200) is None
    True
    >>> windChill(-100, 0) is None
    True
    """
    if degF > 50.0 or windMPH < 3.0:
        return None
    else:
        return 35.74 + (0.6215 * degF) - 35.75 * math.pow(windMPH, 0.16) + (0.4275 * degF) * math.pow(windMPH, 0.16)


def temperatureHumidityIndex(degF, rh):
    """
    Taken from "A New Empirical Model of the Temperature–Humidity Index" by Carl Schoen
    http://journals.ametsoc.org/doi/full/10.1175/JAM2285.1
    >>> round(temperatureHumidityIndex(100, 50))
    112.0
    >>> round(temperatureHumidityIndex(80,30))
    78.0
    """
    d = dewpoint(degF, rh)
    t = degF
    return t - 0.9971 * math.exp(0.02086 * t) * (1 - math.exp(0.0445 * (d - 57.2)))


def angularMean(angles):
    """
    Formula taken from http://en.wikipedia.org/wiki/Mean_of_circular_quantities
    Returns (angle, magnitude).
    >>> angularMean([45,45,45])
    (45.0, 1.0)
    >>> angularMean([0, 180])[1] < 0.0001
    True
    """
    xacc = 0.0
    yacc = 0.0
    count = 0.0
    for angle in angles:
        xacc += math.cos(math.radians(angle))
        yacc += math.sin(math.radians(angle))
        count += 1.0
    if count == 0:
        return None
    x = xacc / count
    y = yacc / count
    magnitude = math.sqrt(math.pow(x, 2) + math.pow(y, 2))
    return ((360.0 + math.degrees(math.atan2(y, x))) % 360, magnitude)


def degreesToCompass(d):
    """
    Takes degrees, returns compass direction.
    >>> [degreesToCompass(x) for x in [0,20,44,45,46,180,270,290,300,355,360,365]]
    ['N', 'N', 'NE', 'NE', 'NE', 'S', 'W', 'W', 'NW', 'N', 'N', 'N']
    """
    directions = 'N NE E SE S SW W NW'.split()
    directions *= 2  # no need for modulo later
    d = (d % 360) + 360 / 16
    return directions[int(d / 45)]


if __name__ == "__main__":
    import doctest
    doctest.testmod()
