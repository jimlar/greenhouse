Greenhouse watering project
===========================

Shopping list
-------------
* [Wemos D1 mini](https://www.wemos.cc/product/d1-mini.html)
* [Wemos OLED shield](https://www.wemos.cc/product/oled-shield.html)
* [Wemos DC power shield](https://www.wemos.cc/product/dc-power-shield.html)

* 2 [Water values](https://www.adafruit.com/products/997)
* 2 [Liquid flow meters](https://www.adafruit.com/products/833)
* 2 TIP120 darlington transistors
* 2 1N4007 diodes
* 2 1KOhm resistors
* A DHT21 sensor, I use the AM2301


Assembly
--------

Hooking up the solenoids to the Wemos D1 mini:

![Solenoids](docs/solenoids.png "Water valve solenoids")


Programming
-----------
* Copy settings_template.h to settings.h and edit with your settings for WIFI and MQTT.
* Upload to wemos with your [PlatformIO](http://platformio.org/)


Enclosure
---------
8-pin connector:
1. +12v
2. 3.3v
3. Flow sensor 1 - D3
4. Flow sensor 2 - D7
5. Water valve 1 - D8 TIP 120
6. Water valve 1 - D5 TIP 120
7. GND
8. GND
