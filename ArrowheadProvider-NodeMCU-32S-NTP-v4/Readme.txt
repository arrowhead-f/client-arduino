#Arrowhead Provider sketch

This sketch does what an Arrowhead service provider system usually does: register its offered services in the ServiceRegistry, and then hosts the resource(s) as an HTTP server.

To get the sketch working, you need to replace the boards.txt and default.csv files for ESP-32S.
See https://github.com/espressif/arduino-esp32/issues/339!

The boards.txt goes to ...\Documents\Arduino\hardware\espressif\esp32,
while the default.csv goes to ...\Documents\Arduino\hardware\espressif\esp32\tools\partitions folders!
