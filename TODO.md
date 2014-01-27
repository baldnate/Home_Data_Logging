TODOS
=====

Big issues
----------

* Outside temp is screwy.  I suspect that hot air is coming out of the gable vent into the enclosure.
	* see http://www.youtube.com/watch?v=KOBt7sxtx0Y for a good shield design
	* add external probe if needed
* Also, the temp sensors were already 2 degrees apart.
* Wind/rain counters have rollover bugs
* Trace rain gets missed due to tick events being longer than 1 day apart
   * when culling, shift the latest culled sample to the window beginning.
* Add all the sensors to wx.ino's output, even if they seem useless or low quality.
	* retrofit ear.ino once wx node is updated with new data
* I suspect that the barometer readings aren't being oversampled.  I need to compare this versus the stock firmware.

Minor issues
------------

* redo mast and grounding
   * ensure correct orientation and level-ness
* rain/wind data compression:
   * elide points that contain no new data (i.e.: more recent timestamp, same number of ticks and direction)
* add PWS secrets to secrets file (once I have the account set up)

Wish list
---------

* shift from time based tweeting to interesting condition change based tweeting
* add summary tweets?  overnight low, gust of the day, etc.
* actually write some design docs
* modularize the arduino code
