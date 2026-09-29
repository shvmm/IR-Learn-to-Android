# IR receiver firmware: ESP32-S3 / GPIO9

Configured for the existing IR Code Studio HTML app:

* ESP32-S3 target
* IR receiver OUT connected to GPIO9
* Active-low demodulated receiver output
* Native USB Serial/JTAG console by default
* 15 ms frame-ending idle threshold

## Build and flash

Extract to a fresh folder. In your activated ESP-IDF master terminal, run from this folder:

```sh
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p PORT flash
```

Replace PORT with the board's actual serial port. In menuconfig, IR Code Studio > IR receiver output GPIO should be 9. Check polarity against your working ESPHome configuration. If using a previously configured build folder, menuconfig must be used to update existing settings: defaults do not overwrite an existing sdkconfig.

The default console uses the S3 native USB connector. If your cable is connected through the board's USB-to-UART bridge instead, choose Component config > ESP System Settings > Channel for console output > Default: UART0, and keep 115200 baud. Select the corresponding port in the browser. Which connector is native USB depends on your specific board.

Close idf.py monitor or other serial monitors before opening the HTML app. Click Connect ESP32, select the port, and wait for the app to show GPIO 9. The previously supplied HTML works without changes. Its embedded generic firmware download still uses the old GPIO4 default; use this GPIO9 package instead.

Keep the receiver's existing working power and ground wiring; its output must be safe for a 3.3 V ESP32 input.

## Validation status

This is firmware source, not a compiled binary. It targets the current ESP-IDF master RMT API, but has not been compiled or hardware-tested in this environment. Record your ESP-IDF commit when building; compatibility with every future master commit cannot be guaranteed.

No Arduino framework or external firmware libraries are required. Captures are receive-only. The browser preserves raw microsecond timings; carrier frequency cannot be measured by an ordinary demodulating receiver.

