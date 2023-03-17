# ESP-based AC Controller

**NOTE:** This is a personal project made for myself for my AC's model (Hitachi R-LT0541-HTA). If you have the same AC, feel free to use this code, otherwise you might have to customize it for your needs.

## Hardware

### Components

1. ESP32/ESP8266 board
2. IR LED
3. Transistor + Resistor (~200 ohm) [optional but recommended]

Hook up the IR LED to GPIO pin 4 (for ESP32, check available for other ESP versions).

**Highly recommended to use a simple BJT NPN transistor to power the LED, since ESPs can't output much current via the GPIO pins**

## How it works

1. [IRRemote8266](https://github.com/crankyoldgit/IRremoteESP8266) library is used to control the IR LED, which functions as a "virtual remote".
2. A small website is created on http://esp32ac.local to control the AC locally without an internet connection.
3. [SinricPro](https://sinric.pro) is used to add Alexa support.

## Credits

1. [esp8266-AC-control](https://github.com/mariusmotea/esp8266-AC-control) for the web UI
2. 