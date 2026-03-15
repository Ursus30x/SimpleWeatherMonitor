# Simple Weather Monitor

A esp32 temperature, humidity and weather monitor that is made by myself, for myself.

Theres nothing special to it - it consist of esp32-C3, 1602LCD screen, BM280/ATH20 sensor and some cables.

It connects to your wifi, gets time from a NTP server and weather info from open-meteo. Everything else is measured locally.

## Build

Used platormIO to build it, you could probably also do it in arduino IDE or any other build system for microcontrollers.

Change `config_template.h` to `config.h` and fill it out with your info.

## Schematic

***TODO: provide a schematic***

But its nothing complicated, sensor and LCD are connected with I2C interface, LCD screen itself is connected with I2C module.

## Tests

I could add some tests, if something breaks...