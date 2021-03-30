# pilight USB Nano v2 (Arduino compatible) 

The pilight USB Nano software allows any computer with an USB port to work with pilight.

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](http://www.gnu.org/licenses/gpl-3.0)

## New v2 firmware features:
 - Can run on any AVR Arduino compatible board, like Arduino UNO, Nano, MEGA, Leonardo, etc. any clock speed allowed.
 - Can run on other platforms like Arduino SAMD boards (DUE, M0, etc.), ESP8266, ESP32, Teensy, even Raspberry Pico. 
 - Works using the MCU's internal USB-Serial COM port (CDC/ACM device class) or any onboard/external USB-Serial adapter.
 - Fully Arduino IDE compiler environment compatible. Arduino PRO IDE and Arduino CLI also supported.
 - Configurable RF receiver output (RX_PIN); must be interrupt attachable, depends board (D2 as default).
 - Configurable RF transmitter input (TX_PIN); can be any digital pin, depends board (D5 as default).
 - Support to configure a digital output so that a led blinks at valid RF code reception.
 - Support to configure send of every 'space' before 'pulse', which stripped in previous version firmware.
 - Support to configure initial RX settings at boot, like as 's:22,200,3000,51000@'.
 - Fix TX pulse generator drift from 9.95µS to 0.69µS (AVR@16Mhz).
 - Improve RX pulse meter resolution from 10µS to 4µS (AVR@16Mhz).

## Usage:

1. Compile the firmware:
  ```
  - Open Arduino IDE application
  - File> Open> Select file "pilight-usb-nano.ino"
  - Tools> Board> Select your board
  - Sketch> Verify/Compile
  ```
2. Flash the firmware on the board:
  ```
  - Tools> Port> Select your USB/Serial COM port
  - Sketch> Upload
  ```
3. Connect the receiver data pin to configured RX_PIN (D2 as default) and the transmitter data pin to configured TX_PIN (D5 as default).
4. Connect the Arduino to your computer.
5. Check what Serial/COM port the Arduino is using.
6. Configure pilight to interface with the Arduino (see below).
7. Start pilight normally and it should work OOTB.

## pilight USB Arduino hardware configuration:

Linux example:
```
	"hardware": {
		"433nano": {
			"comport": "/dev/ttyUSB0"
		}
	}
```
Windows Example:
```
	"hardware": {
		"433nano": {
			"comport": "COM5"
		}
	}
```

## Example:
Transmitter code:
```
c:011010100101011010100110101001100110010101100110101010101010101012;p:1400,600,6800;r:4@
```
  * Receive with spaces:
    ```
    c:011010100101011010100110101001100110010101100110101010101010101012;p:1400,600,6800@
    ```

  * Receive without spaces:
    ```
    c:100011100010001010111010000000002;p:1400,600,6800@
    ```

## Decoding:
You can decode and encode any code using an external tool **"picoder"** from https://github.com/latchdevel/picoder

Decode example:
```
$ picoder decode -s "c:011010100101011010100110101001100110010101100110101010101010101012;p:1400,600,6800@"
```
return:
```
  [{
    "conrad_rsl_switch": {
      "id": 1,
      "unit": 2,
      "state": "on"
    }
  }]
```

Encode example:
```
$ picoder encode -f '{ "conrad_rsl_switch" : {"id":1,"unit":2,"on":1} }' -r 5
```
return:
```
c:011010100101011010100110101001100110010101100110101010101010101012;p:1400,600,6800;r:5@
```

## To do:
- [ ] Support to measure RSSI of receivers that provide it.

# License

Copyright (C) 2015 CurlyMo. This file is part of pilight. GPLv3.

Copyright (C) 2021 Jorge Rivera. GNU General Public License v3.0.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


