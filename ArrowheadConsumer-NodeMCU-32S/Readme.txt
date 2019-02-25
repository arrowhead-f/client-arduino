# Arrowhead consumer sketch

This sketch does what an Arrowhead system is doing when it wishes to consume a service: request orchestration and the connect to the returned endpoint(s). 

To get the sketch working, you need to replace the boards.txt and default.csv files for ESP-32S.
See https://github.com/espressif/arduino-esp32/issues/339!

The boards.txt goes to ...\Documents\Arduino\hardware\espressif\esp32,
while the default.csv goes to ...\Documents\Arduino\hardware\espressif\esp32\tools\partitions folders!
