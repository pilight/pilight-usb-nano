/*
	Copyright (C) 2015 CurlyMo
  This file is part of pilight. GNU General Public License v3.0.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>

  Copyright (C) 2021 Jorge Rivera. GNU General Public License v3.0.
  New v2 firmware features:
   - Fully Arduino IDE compiler environment compatible. Arduino PRO IDE and Arduino CLI also supported.
   - Configurable RF receiver output (RX_PIN); must be interrupt captable, depends board (D2 as default).
   - Configurable RF transmitter input (TX_PIN); can be any digital pin, depends board (D5 as default).

*/ 

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#if !defined(__AVR_ATmega168P__) and !defined(__AVR_ATmega328P__)
#error "MCU must be AVR ATmega168p or ATmega328p"
#endif 

#if (F_CPU != 16000000)
#error "MCU clock must be 16Mhz"
#endif

/* Configurable RX & TX pins */
#define RX_PIN                2     // Pin for ASK/OOK pulse input from RF receiver module data output.
#define TX_PIN                5     // Pin for ASK/OOK pulse output to RF transmitter module data input.

#define BUFFER_SIZE 					256
#define MAX_PULSE_TYPES				10
#define BAUD									57600
/* Number devided by 10 */
#define MIN_PULSELENGTH 			8			//tested to work down to 30us pulsewidth (=2)
#define MAX_PULSELENGTH 			1600
#define VERSION								2

volatile uint32_t minrawlen = 1000;
volatile uint32_t maxrawlen = 0;
volatile uint32_t mingaplen = 10000;
volatile uint32_t maxgaplen = 5100;

// Code formatting meant for sending
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486;r:5@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486;r:5@

// Code formatting outputted by receiver
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486@

char data[BUFFER_SIZE] = {0};                       // Fill to 0 // Buffer for serial uart inputs and outputs
volatile unsigned long ten_us_counter1 = 0;
volatile uint16_t ten_us_counter = 0, codes[BUFFER_SIZE], plstypes[MAX_PULSE_TYPES] = {0};
volatile uint8_t state = 0, codelen = 0, repeats = 0, pos = 0;
volatile uint8_t valid_buffer = 0x00, r = 0, q = 0, rawlen = 0, nrpulses = 0;

void ISR_RX(); // Generic ISR function declaration for RF RX pulse interrupt handler instead specific AVR ISR(vector, attributes)

void setup() {

	cli();

  pinMode(TX_PIN, OUTPUT);

	// TIMER = (F_CPU / PRESCALER)
	// OCR = ((F_CPU / PRESCALER) * SECONDS) - 1

  /* *** TIMER2 CONFIG TCCR2A *** 
  Original TCCR2A = TCCR2A | (1 << WGM21);
  Expected TCCR2A = 0b0000 0010 --> Mode 2 (CTC)

  TCCR2A – Timer/Counter Control Register A, Initial Value 0
  Bits 1:0 – WGM21:0: Waveform Generation Mode

  WARNING! Arduino set WGM20 in /cores/arduino/wiring.c
  #if defined(TCCR2A) && defined(WGM20)
	  sbi(TCCR2A, WGM20);
  */

  bitSet(TCCR2A, WGM21); bitClear(TCCR2A, WGM20);

  /* *** TIMER2 CONFIG TCCR2B ***
  Original TCCR2B = TCCR2B | (1 << CS21);
  Expected TCCR2B = 0b0000 0010 --> clkT2S/8 (from prescaler)

  TCCR2B – Timer/Counter Control Register B, Initial Value 0
  Bit 2:0 – CS22:0: Clock Select

  WARNING! Arduino set CS22 in /cores/arduino/wiring.c
  #if defined(TCCR2B) && defined(CS22)
	  sbi(TCCR2B, CS22);
  */

  bitClear(TCCR2B, CS22); bitSet(TCCR2B, CS21); bitClear(TCCR2B, CS20);

  /* *** SET COMPARE FLAG ***
  OCF2A: Output Compare Flag 2 A
  The OCF2A bit is set (one) when a compare match occurs between the Timer/Counter2 and the data in OCR2A – output
  compare register2. OCF2A is cleared by hardware when executing the corresponding interrupt handling vector.
  Alternatively, OCF2A is cleared by writing a logic one to the flag. When the I-bit in SREG, OCIE2A (Timer/Counter2 compare
  match interrupt enable), and OCF2A are set (one), the Timer/Counter2 compare match interrupt is executed.
  */

  OCR2A = 0x13;

  /* *** ENABLE TIMER2 COMPARE INTERRUPT ***
  TIMSK2 – Timer/Counter2 Interrupt Mask Register
  OCIE2A: Timer/Counter2 Output Compare Match A Interrupt Enable

  TIMSK2 – Timer/Counter2 Interrupt Mask Register, Initial Value 0
  Bit 1 – OCIE2A: Timer/Counter2 Output Compare Match A Interrupt Enable

  When the OCIE2A bit is written to one and the I-bit in the status register is set (one), the Timer/Counter2 compare match A
  interrupt is enabled. The corresponding interrupt is executed if a compare match in Timer/Counter2 occurs, i.e., when the
  OCF2A bit is set in the Timer/Counter 2 interrupt flag register – TIFR2
  */
  
  bitSet(TIMSK2, OCIE2A); 

  // Arduino built-in function to attach Interrupt Service Routines (depends board)
	attachInterrupt(digitalPinToInterrupt(RX_PIN), ISR_RX, CHANGE);
	
  // Arduino build-in function to set serial UART data baud rate (depends board)
	Serial.begin(BAUD);

	sei();
}

/* Everything is parsed on-the-fly to preserve memory */
void receive() {
	unsigned int scode = 0, spulse = 0, srepeat = 0, sstart = 0;
	unsigned int i = 0, s = 0, z = 0, x = 0;

	nrpulses = 0;

	z = strlen(data);
	for(i = 0; i < z; i++) {
		if(data[i] == 's') {
			sstart = i + 2;
			break;
		}
		if(data[i] == 'c') {
			scode = i + 2;
		}
		if(data[i] == 'p') {
			spulse = i + 2;
		}
		if(data[i] == 'r') {
			srepeat = i + 2;
		}
		if(data[i] == ';') {
			data[i] = '\0';
		}
	}
	/*
	 * Tune the firmware with pilight-daemon values
	 */
	if(sstart > 0) {
		z = strlen(&data[sstart]);
		s = sstart;
		x = 0;
		for(i = sstart; i < sstart + z; i++) {
			if(data[i] == ',') {
				data[i] = '\0';
				if(x == 0) {
					minrawlen = atol(&data[s]);
				}
				if(x == 1) {
					maxrawlen = atol(&data[s]);
				}
				if(x == 2) {
					mingaplen = atoi(&data[s])/10;
				}
				x++;
				s = i+1;
			}
		}
		if(x == 3) {
			maxgaplen = atol(&data[s])/10;
		}
		/*
		 * Once we tuned our firmware send back our settings + fw version
		 */
		sprintf(data, "v:%lu,%lu,%lu,%lu,%d,%d,%d@", minrawlen, maxrawlen, mingaplen*10, maxgaplen*10, VERSION, MIN_PULSELENGTH, MAX_PULSELENGTH);
		Serial.print(data);
	} else if(scode > 0 && spulse > 0 && srepeat > 0) {
		z = strlen(&data[spulse]);
		s = spulse;
		nrpulses = 0;
		for(i = spulse; i < spulse + z; i++) {
			if(data[i] == ',') {
				data[i] = '\0';
				plstypes[nrpulses++] = atoi(&data[s]);
				s = i+1;
			}
		}
		plstypes[nrpulses++] = atoi(&data[s]);

    /* Begin RF TX */
		codelen = strlen(&data[scode]);
		repeats = atoi(&data[srepeat]);
    // Disable all interrupts
		noInterrupts(); 
		for(i=0;i<repeats;i++) {
			for(z = scode; z < scode + codelen; z++) {
				digitalWrite(TX_PIN,!(z%2));
				delayMicroseconds(plstypes[data[z] - '0'] - 14);  // subtract 14us to compensate digitalWrite() delay      
			}
		}
		digitalWrite(TX_PIN,LOW);

		for(r=0;r<MAX_PULSE_TYPES;r++) {
			plstypes[r] = 0;
		}
		q = 0;

    // Re-Enable all interrupts
	  interrupts();
    /* End RF TX */
	}
}

// Arduino build-in function called by INT when serial data is available (depends board)
void serialEvent() {
  // get the new byte
  char c = (char)Serial.read();
  // add it to the inputString
  data[q++] = c;
  // if the incoming character is a @ call receive()
	if(c == '@') {
	  data[q++] = '\0';
	  receive();
	  q = 0;
	}
}

/* Our main timer */
ISR(TIMER2_COMPA_vect) {
	cli();
	ten_us_counter++;
	ten_us_counter1++;
	if(ten_us_counter1 > 100000) {	
		Serial.println();
		ten_us_counter1 = 0;
	}
	sei();
}

void broadcast() {
	int i = 0, x = 0, match = 0, p = 0;

	Serial.print("c:");
	for(i=0;i<nrpulses;i++) {
		match = 0;
		for(x=0;x<MAX_PULSE_TYPES;x++) {
			/* We device these numbers by 10 to normalize them a bit */
			if(((plstypes[x]/10)-(codes[i]/10)) <= 2) {
				/* Every 'space' is followed by a 'pulse'.
				 * All spaces are stripped to spare
				 * resources. The spaces can easily be
				 * added afterwards.
				 */
				if((i%2) == 1) {
					/* Write numbers */
					Serial.print(char(48+x));
				}
				match = 1;
				break;
			}
		}
		if(match == 0) {
			plstypes[p++] = codes[i];
			/* See above */
			if((i%2) == 1) {
				Serial.print(char(48+p-1));
			}
		}
	}
	Serial.print(";p:");
	for(i=0;i<p;i++) {
    Serial.print(plstypes[i]*10,DEC);
		if(i+1 < p) {
			Serial.print(',');
		}
		plstypes[i] = 0;
	}
	Serial.print('@');
	nrpulses = 0;
}

// Generic ISR function for RF RX pulse interrupt handler
void ISR_RX(){
  // Disable ISR for RF RX interrupt handler only
  detachInterrupt(digitalPinToInterrupt(RX_PIN));

	/* We first do some filtering (same as pilight BPF) */
	if(ten_us_counter > MIN_PULSELENGTH) {
		if(ten_us_counter < MAX_PULSELENGTH) {
			/* All codes are buffered */
			codes[nrpulses++] = ten_us_counter;
			if(nrpulses > BUFFER_SIZE) {
				nrpulses = 0;
			}
			/* Let's match footers */
			if(ten_us_counter > mingaplen) {
				/* Only match minimal length pulse streams */
				if(nrpulses >= minrawlen && nrpulses <= maxrawlen) {
					/*
					 * Sending pulses over serial requires
					 * a lot of cpu ticks. We therefor have
					 * to be sure that we send valid codes.
					 * Therefor, only streams we at least 
					 * received twice communicated.
					 */
					if(rawlen == nrpulses) {
						broadcast();
					}
					rawlen = nrpulses;
				}
				nrpulses = 0;
			}
		}
	}
	ten_us_counter = 0;

  // Re-enable ISR for RF RX interrupt handler
  attachInterrupt(digitalPinToInterrupt(RX_PIN), ISR_RX, CHANGE);
}

void loop(){
  // put your main code here, to run repeatedly:
}
