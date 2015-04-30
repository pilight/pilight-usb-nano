# pilight USB Nano

The pilight Arduino Nano software allows any computer with an USB port to work with pilight.

1. Compile the firmware:
```
avr-gcc -Os -Wall -DF_CPU=16000000UL -mmcu=atmega328p -c -o pilight_usb_nano.o pilight_usb_nano.c -lm -I.
avr-gcc -mmcu=atmega328p pilight_usb_nano.o -o pilight_usb_nano
avr-objcopy -O ihex -R .eeprom pilight_usb_nano pilight_usb_nano.hex
```
2. Flash the firmware on the Arduino Nano:
`avrdude -b 57600 -p atmega328p -c arduino -P COM5 -U flash:w:pilight_usb_nano.hex`
3. Connect the receiver data pin to D2 and the sender to D5.
4. Connect the Arduino Nano to your computer.
5. Check what COM port the Arduino Nano is using.
6. Configure pilight to interface with the Arduino Nano (see below).
7. Start pilight normally and it should work OOTB.

pilight USB nano hardware configuration:

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