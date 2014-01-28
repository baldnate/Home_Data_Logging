# wx_pws.py - by baldnate
#
# Wraps actually talking to wunderground's PWS system
# based of info from
# http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol

import urllib


class WundergroundPWS(object):

    """docstring for WundergroundPWS"""

    def __init__(self, account, password, rtfreq=False):
        super(WundergroundPWS, self).__init__()
        self.secrets = {
            'ID': account,
            'PASSWORD': password
        }
        self.stock = {
            'action': 'updateraw',
            'softwaretype': 'baldwx'
        }
        self.realtime = bool(rtfreq)
        if self.realtime:
            self.stock['realtime'] = 1
            self.stock['rtfreq'] = rtfreq

    def update(self, **kwargs):
        obs = dict((k, v) for k, v in kwargs.iteritems() if v is not None)
        args = dict(obs.items() + self.stock.items() + self.secrets.items())
        params = urllib.urlencode(args)
        bld = ""
        if self.realtime:
            bld = 'rtupdate'
        else:
            bld = 'weatherstation'
        f = urllib.urlopen(
            "http://%s.wunderground.com/weatherstation/updateweatherstation.php?%s" % (bld, params))
        result = f.read()
        if 'success' in result:
            return
        else:
            raise Exception(result)

if __name__ == "__main__":
    import doctest
    doctest.testmod()
