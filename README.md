# IR-Learn-to-Android
Learn IR Remote through Web-Serial and ESP32+IR Sensor, and then import to IR Blaster Remote Android app https://github.com/iodn/android-ir-blaster.

Using this app is extremely simple:

0. All you need is an ESP32/ESP32-S3 flashed with IR receiver firmware compiled from the folder in repo.

The firmware is a standard ESP-IDF project.

Verified with:
Seeed Studio XIAO S3
IR Receiver: https://robu.in/product/infrared-receiver-module-arduino/

ESP-IDF 6.2.0 master @ https://github.com/espressif/esp-idf

1. Once you've flashed the device, Run the standalone `ir-code-studio.html` app.
Everything is straightforward from there.
You create name for a new remote and then add buttons to it by pointing your physical remote to the IR receiver.
Then you can export your JSON for further use or you can export to Android IR Blaster format for importing to any IR equipped Android Phone. I tested with a POCO X7 Pro.
I think any phone worth its salt in 2026 should come with an IR transmitter in the rear camera island. 

I'm especially proud of the Sequential verification feature that will let you verify your capture quickly.