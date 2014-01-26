# ez_tweet.py - by baldnate
#
# When you just want to update twitter status and nothing else, ez_tweet is there.

from twython import Twython, TwythonError, TwythonAuthError

class EZTweet(object):
	"""docstring for EZTweet"""
	def __init__(self, appkey, appsecret, oauthtoken, oauthtokensecret):
		super(EZTweet, self).__init__()
		self.twitter = Twython(appkey, appsecret, oauthtoken, oauthtokensecret)
		self.lastTweet = ""

	def __what_to_do__(self, e):
		"""
		Internal function for figuring out what to do with a Twython exception
		"""
		if e is TwythonAuthError:
			raise e
		if e is TwythonStreamError:
			raise e
		if e is TwythonRateLimitError:
			return e.retry_after
		if e.error_code in [502, 503, 504]:
			return 60
		else:
			raise e

	def tweet(self, status):
		"""
		Returns number of seconds to wait until next tweet.
		Raises if a non-retryable offense occurs.
		"""
		if status == self.lastTweet:
			return 0
		try:
			self.twitter.update_status(status=status)
		except TwythonError as e:
			return self.__what_to_do__(e)
		self.lastTweet = status
		return -1

if __name__ == "__main__":
    import doctest
    doctest.testmod()