TODOS
=====

host/wx_bridge.py
-----------------

* heat index / wind chill
* wind related    
   * consider mean of circular quantities for gust direction?
* dew-point
* implement non-numeric reports (rain == trace, etc.)
* shift from time based tweeting to interesting condition change based tweeting
* add summary tweets?  overnight low, gust of the day, etc.
* conversion from pressure in absolute pascals to altimeter setting inches
* rain/wind data compression:
   * elide points that contain no new data (i.e.: more recent timestamp, same number of ticks and direction)
   * when culling, shift the latest culled sample to the window beginning.
* posting to wunderground PWS
   * add PWS secrets to secrets file
* create prefs files for non-secret config
   * create example prefs and secrets for documentation purposes
   * scheduling info?
* consider modularizing once feature set is more stable

host/serial_capture.py
----------------------

* parameterize serial port
* add banner

host/send_tweet.py
------------------

* modularize
* retrofit wx_bridge to use this instead of just rolling its own

documentation/*
---------------

* actually write some design docs

firmware/wx/wx.ino
------------------

* I suspect that the barometer readings aren't being oversampled.  I need to compare this versus the stock firmware.
* Add all the sensors to the output, even if they seem useless or low quality.
* modularize the parts that seem stable
* capture fastest wind impulse (shortest tick to tick time) to better capture gusts when packet loss is high

firmware/ear/ear.ino
--------------------

* retrofit once wx node is updated with new data

non-code
--------

* add external probe? (https://www.sparkfun.com/products/11050)
* improve air flow to enclosure?
* redo mast and grounding
   * ensure correct orientation and level-ness
* fool around with antennas to reduce packet loss
