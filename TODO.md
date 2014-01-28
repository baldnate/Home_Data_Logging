TODOS
=====

Big issues
----------

* Outside temp is screwy.  I suspect that hot air is coming out of the gable vent into the enclosure.
	* siting guide: http://wiki.wunderground.com/index.php/PWS_-_Siting
	* see http://www.youtube.com/watch?v=KOBt7sxtx0Y for a diy shield design
	* Also note: the humidity and pressure temp sensors were already ~2 degF apart.
	* add external temp probe if needed
* I suspect that the barometer readings aren't being oversampled.  I need to compare this versus the stock firmware.

Minor issues
------------

* redo mast and grounding
   * ensure correct orientation and level-ness
* rain/wind data compression:
   * elide points that contain no new data (i.e.: more recent timestamp, same number of ticks and direction)

Wish list
---------

* shift from time based tweeting to interesting condition change based tweeting
* add summary tweets?  overnight low, gust of the day, etc.
* actually write some design docs
* modularize the arduino code
