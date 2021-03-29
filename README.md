# pilight USB Nano v2 (Arduino compatible) 

The pilight USB Nano software allows any computer with an USB port to work with pilight.

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](http://www.gnu.org/licenses/gpl-3.0)

## New v2 firmware features:
 - Can run on any AVR Arduino compatible board, like Arduino UNO, Nano, MEGA, Leonardo, etc.
 - Can run on other platforms like Arduino DUE, M0 (any SAMD boards), ESP8266, ESP32, Teensy, even Raspberry Pico. 
 - Fully Arduino IDE compiler environment compatible. Arduino PRO IDE and Arduino CLI also supported.
 - Configurable RF receiver output (RX_PIN); must be interrupt attachable, depends board (D2 as default).
 - Configurable RF transmitter input (TX_PIN); can be any digital pin, depends board (D5 as default).

## Usage:

1. Compile the firmware:
  ```
  - Open Ardino IDE application
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


