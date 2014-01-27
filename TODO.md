TODOS
=====

host/wx_bridge.py
-----------------

* wind rollover bug needs fixing
* trace rain gets missed due to tick events being longer than 1 day apart
   * when culling, shift the latest culled sample to the window beginning.
* rain/wind data compression:
   * elide points that contain no new data (i.e.: more recent timestamp, same number of ticks and direction)
* add PWS secrets to secrets file
* shift from time based tweeting to interesting condition change based tweeting
* add summary tweets?  overnight low, gust of the day, etc.

host/serial_capture.py
----------------------

* parameterize serial port
* add banner

documentation/*
---------------

* actually write some design docs

firmware/wx/wx.ino
------------------

* I suspect that the barometer readings aren't being oversampled.  I need to compare this versus the stock firmware.
* Add all the sensors to the output, even if they seem useless or low quality.
* modularize the parts that seem stable

firmware/ear/ear.ino
--------------------

* retrofit once wx node is updated with new data

non-code
--------

* test/add external probe
* build better enclosure (see http://www.youtube.com/watch?v=KOBt7sxtx0Y)
* redo mast and grounding
   * ensure correct orientation and level-ness
