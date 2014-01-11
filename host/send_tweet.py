from twython import Twython
import json

secrets = json.load(open('secrets.json'))

twitter = Twython(secrets['APP_KEY'], secrets['APP_SECRET'], secrets['OAUTH_TOKEN'], secrets['OAUTH_TOKEN_SECRET'])

twitter.update_status(status="TESTING TESTING.")