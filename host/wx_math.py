# encoding: utf-8

# wx_math.py - dumping ground for a bunch of weather related math
#
# By baldnate with credit noted in individual functions

import math

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
    """
    a = (pascals / 100.0) - 0.3 # Pmb - 0.3
    h = altitudeInMeters
    return mbToInchesHg(math.pow(((h / math.pow(a, 0.190284)) * 0.000084228806861) + 1, 5.255302600323727) * a)

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
    """
    if degF > 50.0 and windMPH < 3.0:
        return None
    else:
        return 35.74 + (0.6215 * degF) - 35.75 * math.pow(windMPH, 0.16) + (0.4275 * degF) * math.pow(windMPH, 0.16)

if __name__ == "__main__":
    import doctest
    doctest.testmod()