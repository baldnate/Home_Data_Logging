# ez_tweet.py - by baldnate
#
# When you just want to update twitter status and nothing else, ez_tweet is there.

from twython import Twython, TwythonError, TwythonAuthError, TwythonStreamError, TwythonRateLimitError


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
        elif e is TwythonStreamError:
            raise e
        elif e is TwythonRateLimitError:
            return e.retry_after
        elif e.error_code in [502, 503, 504]:
            return 1 * 60
        elif e.error_code in [403]:
            return 5 * 60
        else:
            raise e

    def tweet(self, status):
        """
        Returns number of seconds to wait until next tweet.
        Raises if a non-retryable offense occurs.
        """
        retVal = -1
        if status == self.lastTweet:
            return 0
        try:
            self.twitter.update_status(status=status)
        except TwythonError as e:
            retVal = self.__what_to_do__(e)
        self.lastTweet = status
        return retVal

if __name__ == "__main__":
    import doctest
    doctest.testmod()
