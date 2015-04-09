/*
	Copyright (C) 2015 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/ 

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#define BUFFER_SIZE 				256
#define MAX_PULSE_TYPES			10
#define BAUD								57600
#define MIN_PULSELENGTH 		8			//tested to work down to 30us pulsewidth (=2)
#define MAX_PULSELENGTH 		1600
#define MIN_FOOTER_LENGTH		510		// 5100 deviced by 10
#define MIN_STREAM_LENGTH		25

#include <util/setbaud.h>

// Code formatting meant for sending
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486;r:5@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486;r:5@


// Code formatting outputted by receiver
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486@

char data[BUFFER_SIZE];
volatile unsigned long ten_us_counter1 = 0;
volatile uint16_t ten_us_counter = 0, codes[BUFFER_SIZE], plstypes[MAX_PULSE_TYPES];
volatile uint8_t state = 0, codelen = 0, repeats = 0, pos = 0;
volatile uint8_t valid_buffer = 0x00, r = 0, q = 0, plslen = 0, nrpulses = 0;

void initUART(void) {
	DDRD |= _BV(PD1);
	DDRD &= ~_BV(PD0);

	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;

#if USE_2X
	UCSR0A |= _BV(U2X0);
#else
	UCSR0A &= ~_BV(U2X0);
#endif	
	
	UCSR0B |= _BV(RXEN0);
	UCSR0B |= _BV(RXCIE0);
	UCSR0B |= _BV(TXEN0);

	UCSR0C |= _BV(USBS0);
	UCSR0C |= _BV(UCSZ01) | _BV(UCSZ00);
}

void setup() {
	uint8_t oldSREG = SREG;

	cli();

	/* We initialize our array to zero
	 * here to spare resources while
	 * running.
	 */
	for(r=0;r<MAX_PULSE_TYPES;r++) {
		plstypes[r] = 0;
	}	

	ADCSRA &= ~_BV(ADEN);
	ACSR = _BV(ACD);
	DIDR0 = 0x3F;
	DIDR1 |= _BV(AIN1D) | _BV(AIN0D);

	power_twi_disable();
	power_spi_disable();
	power_timer0_disable();
	power_timer1_disable();
	//power_timer2_disable();

	DDRD |= _BV(DDD5);
	DDRB |= _BV(DDB5);
	SREG = oldSREG;

	// TIMER = (F_CPU / PRESCALER)
	// OCR = ((F_CPU / PRESCALER) * SECONDS) - 1
	OCR2A = 0x13;
	TIMSK2 |= _BV(OCIE2A);
	TCCR2A = TCCR2A | (1 << WGM21);
	TCCR2B = TCCR2B | (1 << CS21);

	PCMSK2 |= _BV(PCINT18);
	PCICR |= _BV(PCIE2);
	
	initUART();
	sei();
}

/* From the Arduino library */
void delayMicroseconds(unsigned int us) {
	if(--us == 0)
		return;

	us <<= 2;
	us -= 2;

	__asm__ __volatile__ (
		"1: sbiw %0,1" "\n\t" // 2 cycles
		"brne 1b" : "=w" (us) : "0" (us) // 2 cycles
	);
}

uint8_t getByte(void) {
	/* Wait for data to be buffer */
	while(!(UCSR0A & (1 << RXC0)));
		return (uint8_t)UDR0;
}

void putByte(unsigned char data) {
	/* Wait for empty transmit buffer */
	while(!(UCSR0A & (1 << UDRE0)));
		UDR0 = (unsigned char) data;
}

/*! \brief Writes an ASCII string to the TX buffer */
void writeString(char *line) {
	while(*line != '\0') {
		putByte(*line);
		++line;
	}
}

char *readString(void) {
	static char rxstr[32];
	static char *temp;
	temp = rxstr;

	while((*temp = getByte()) != '\n') {
		++temp;
	}

	return rxstr;
}

void flush(void) {
	unsigned char dummy;
	while(UCSR0A & (1 << RXC0)) {
		dummy = UDR0;
	}
}

/* Everything is parsed on-the-fly to preserve memory */
void sendData() {
	unsigned int scode = 0, spulse = 0, srepeat = 0;
	unsigned int i = 0, s = 0, z = 0;

	nrpulses = 0;

	z = strlen(data);
	for(i = 0; i < z; i++) {
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
	if(scode > 0 && spulse > 0 && srepeat > 0) {
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

		codelen = strlen(&data[scode]);
		repeats = atoi(&data[srepeat]);
		cli();
		for(i=0;i<repeats;i++) {
			for(z = scode; z < scode + codelen; z++) {
				PORTD ^= _BV(PORTD5); 
				delayMicroseconds(plstypes[0]); 
				PORTD ^= _BV(PORTD5); 
				delayMicroseconds(plstypes[data[z] - '0']);      
			}
		}
		PORTD &= ~_BV(PORTD5);

		for(r=0;r<MAX_PULSE_TYPES;r++) {
			plstypes[r] = 0;
		}
		q = 0;

		sei();
	}
}

ISR(USART_RX_vect) {
	char c = UDR0;
	data[q++] = c;
	if(c == '@') {
		data[q++] = '\0';
		sendData();
		q = 0;
	}
}

/* Our main timer */
ISR(TIMER2_COMPA_vect) {
	cli();
	ten_us_counter++;
	ten_us_counter1++;
	if(ten_us_counter1 > 100000) {
		putByte('\n');
		ten_us_counter1 = 0;
	}
	sei();
}

void broadcast() {
	int i = 0, x = 0, match = 0, p = 0;

	putByte('c');
	putByte(':');
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
					putByte(48+x);
				}
				match = 1;
				break;
			}
		}
		if(match == 0) {
			plstypes[p++] = codes[i];
			/* See above */
			if((i%2) == 1) {
				putByte(48+p-1);
			}
		}
	}
	putByte(';');
	putByte('p');
	putByte(':');
	for(i=0;i<p;i++) {
		itoa(plstypes[i]*10, data, 10);
		writeString(data);
		if(i+1 < p) {
			putByte(',');
		}
		plstypes[i] = 0;
	}
	putByte('@');
	nrpulses = 0;
}

ISR(PCINT2_vect){
	cli();
	/* We first do some filtering (same as pilight BPF) */
	if(ten_us_counter > MIN_PULSELENGTH) {
		if(ten_us_counter < MAX_PULSELENGTH) {
			/* All codes are buffered */
			codes[nrpulses++] = ten_us_counter;
			if(nrpulses > BUFFER_SIZE) {
				nrpulses = 0;
			}
			/* Let's match footers */
			if(ten_us_counter > MIN_FOOTER_LENGTH) {
				/* Only match minimal length pulse streams */
				if(nrpulses >= MIN_STREAM_LENGTH) {
					/*
					 * Sending pulses over serial requires
					 * a lot of cpu ticks. We therefor have
					 * to be sure that we send valid codes.
					 * Therefor, only streams we at least 
					 * received twice communicated.
					 */
					if(plslen == nrpulses) {
						broadcast();
					}
					plslen = nrpulses;
				}
				nrpulses = 0;
			}
		}
	}
	ten_us_counter = 0;
	TCNT1 = 0;
	sei();
}

int main(void) {
	setup();
	while(1);
}
