TODOS
=====

Big issues
----------

* Outside temp is *still* screwy.  Either heat coming off the roof is getting into the enclosure, or the solar shield is not doing its job.  I'm considering resiting the entire project to somewhere that isn't the roof.
	* siting guide: http://wiki.wunderground.com/index.php/PWS_-_Siting
	* see http://www.youtube.com/watch?v=KOBt7sxtx0Y for a diy shield design
	* Also note: the humidity and pressure temp sensors were already ~2 degF apart.
* Something is wrong with the serial link.  I am now suspecting that software flow control is needed.  The symptoms are lots of truncated packets (caught by json parser yacking) and a few garbled packets (caught by main code not finding the right keys in the packet).

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
