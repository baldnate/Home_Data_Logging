# wx_pws.py - by baldnate
#
# Wraps actually talking to wunderground's PWS system
# based of info from http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol

import urllib

class WundergroundPWS(object):
	"""docstring for WundergroundPWS"""
	def __init__(self, account, password):
		super(WundergroundPWS, self).__init__()
		self.stock = {
			'action': 'updateraw',
			'softwaretype': 'baldwx'
		}
		self.secrets = {
			'ID': account,
			'PASSWORD': password
		}

	def update(self, **kwargs):
		obs = dict((k, v) for k, v in kwargs.iteritems() if v is not None)
		args = dict(obs.items() + stock.items() + secrets.items())
		params = urllib.urlencode(args)
		f = urllib.urlopen("http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?%s" % params)
		result = f.read()
		if result == 'success':
			return
		else:
			raise Exception(result)
		
if __name__ == "__main__":
	import doctest
	doctest.testmod()
