# wx_pws.py - by baldnate
#
# Wraps actually talking to wunderground's PWS system
# based of info from
# http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol

import requests


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
        args = dict(kwargs.items() + self.stock.items() + self.secrets.items())
        bld = ""
        if self.realtime:
            bld = 'rtupdate'
        else:
            bld = 'weatherstation'
        try:
            r = requests.get("http://%s.wunderground.com/weatherstation/updateweatherstation.php" % bld, params=args)
        except requests.exceptions.ConnectionError as e:
            return
        if 'success' in r.text:
            return
        elif '502' in r.text:
        	return
        elif '408' in r.text:
        	return
        else:
            raise Exception(r.text)

if __name__ == "__main__":
    import doctest
    doctest.testmod()
