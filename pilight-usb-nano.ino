/*
  Copyright (C) 2015 CurlyMo
  This file is part of pilight. GNU General Public License v3.0.

  You should have received a copy of the GNU General Public License
  along with pilight. If not, see <http://www.gnu.org/licenses/>

  Copyright (C) 2021 Jorge Rivera. GNU General Public License v3.0.

  New v2 firmware features:
   - Can run on any AVR Arduino compatible board, like Arduino UNO, Nano, Mega, Leonardo, etc.
   - Can run on other platforms like Arduino SAMD boards (DUE, M0, etc.), ESP8266, ESP32, Teensy, even Raspberry Pico.
   - Fully Arduino IDE compiler environment compatible. Arduino PRO IDE and Arduino CLI also supported.
   - Configurable RF receiver output (RX_PIN); must be interrupt attachable, depends board (D2 as default).
   - Configurable RF transmitter input (TX_PIN); can be any digital pin, depends board (D5 as default).
   - Support to configure a digital output so that a led blinks at valid RF code reception.
   - Support to configure send of every 'space' before 'pulse', which stripped in previous version firmware.
   - Support to configure initial RX settings at boot, like as 's:22,200,3000,51000@'.
   - Support to configure show settings at boot, like as: 'v:22,200,3000,51000,2,8,3600@'.
   - Support to configure add line feed '\n' each line output.

*/ 

/* Configurable RX & TX pins */
#define RX_PIN                2     // Pin for ASK/OOK pulse input from RF receiver module data output.
#define TX_PIN                5     // Pin for ASK/OOK pulse output to RF transmitter module data input.

//#define EVERY_SEC_LINE_FEED       // If defined, print line feed '\n' every second, to emulate legacy firmware.
#define SEND_STRIPPED_SPACES        // If defined, send every 'space' before 'pulse' in broadcast(), which stripped in legacy firmware.
#define LED_BLINK_RX LED_BUILTIN    // If defined, sets the digital output to blink on valid RF code reception.
#define DEFAULT_RX_SETTINGS         // If defined, sets valid RX settings at boot, like sets 's:20,200,3000,51000@'
#define BOOT_SHOW_SETTINGS          // If defined, show settings at boot, like as: 'v:20,200,3000,51000,2,1,1600@'
#define ADD_LINE_FEED               // If defined, add line feed '\n' each line output.

#define BUFFER_SIZE           256   // Warning: 256 max because buffer indexes "nrpulses" and "q" are "uint8_t" type
#define MAX_PULSE_TYPES        10   // From 0 to 9
#define BAUD                57600

/* Show numbers devided by 10 */
#define MIN_PULSELENGTH        10   // v2 change from 8 to 10 and show devided by 10
#define MAX_PULSELENGTH     16000   // v2 change from 1600 to 16000 and show devided by 10

#define VERSION                 2   // Version 2 (Arduino compatible)

#ifdef DEFAULT_RX_SETTINGS
uint32_t minrawlen =    20;
uint32_t maxrawlen =   200;
uint32_t mingaplen =   300;         // Used and showing multiplied by 10
#else
uint32_t minrawlen =  1000;
uint32_t maxrawlen =     0;
uint32_t mingaplen = 10000;         // Used and showing multiplied by 10
#endif 

uint32_t maxgaplen = 5100;          // Unused. Preserved for legacy compatibility.

// Code formatting meant for sending
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486;r:5@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486;r:5@

// Code formatting outputted by receiver
// on  c:102020202020202020220202020020202200202200202020202020220020202203;p:279,2511,1395,9486@
// off c:102020202020202020220202020020202200202200202020202020202020202203;p:279,2511,1395,9486@

char data[BUFFER_SIZE] = {0};                       // Fill to 0 // Buffer for serial uart inputs and outputs
volatile uint16_t codes[BUFFER_SIZE] = {0};         // Fill to 0 // Buffer to store pulses length
         uint16_t plstypes[MAX_PULSE_TYPES] = {0};  // Fill to 0 // Buffer to store pulse types (RX and TX)
volatile uint32_t new_counter = 0;                  // Global time counter to store initial pulse micros(). Replaces global ten_us_counter.

volatile uint8_t q = 0;                             // Index of data buffer
volatile uint8_t rawlen = 0;                        // Flag to ensure to call broadcast() after reveive two same lenght pulse train
volatile uint8_t nrpulses = 0;                      // Index of pulse lenght buffer
volatile uint8_t broadcast_flag = 0;                // Flag to call broadcast with nrpulses value
volatile uint8_t receive_flag = 0;                  // Flag to call receive() in loop()

void ISR_RX(); // Generic ISR function declaration for RF RX pulse interrupt handler instead specific AVR ISR(vector, attributes)

void setup() {

  pinMode(TX_PIN, OUTPUT);

#ifdef LED_BLINK_RX
  pinMode(LED_BLINK_RX, OUTPUT);
#endif

  // Arduino built-in function to attach Interrupt Service Routines (depends board)
  attachInterrupt(digitalPinToInterrupt(RX_PIN), ISR_RX, CHANGE);
    
  // Arduino build-in function to set serial UART data baud rate (depends board)
  Serial.begin(BAUD);

#ifdef BOOT_SHOW_SETTINGS
  // Show settings at boot, like as: 'v:22,200,3000,51000,2,8,3600@'
  sprintf(data, "v:%lu,%lu,%lu,%lu,%d,%d,%d@", minrawlen, maxrawlen, mingaplen*10, maxgaplen*10, VERSION, MIN_PULSELENGTH/10, MAX_PULSELENGTH/10);
#ifdef ADD_LINE_FEED
  Serial.println(data);
#else
  Serial.print(data);
#endif
#endif

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
        sprintf(data, "v:%lu,%lu,%lu,%lu,%d,%d,%d@", minrawlen, maxrawlen, mingaplen*10, maxgaplen*10, VERSION, MIN_PULSELENGTH/10, MAX_PULSELENGTH/10);
#ifdef ADD_LINE_FEED
        Serial.println(data);
#else
        Serial.print(data);
#endif
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
    // Disable all interrupts
        noInterrupts(); 
        for(i=0;i<(unsigned int)atoi(&data[srepeat]);i++) {
            for(z = scode; z < scode + strlen(&data[scode]); z++) {
                digitalWrite(TX_PIN,!(z%2));
                delayMicroseconds(plstypes[data[z] - '0'] - 14);  // subtract 14us to compensate digitalWrite() delay      
            }
        }
        digitalWrite(TX_PIN,LOW);

    // Clear pulse types array
        for(i=0;i<MAX_PULSE_TYPES;i++) {
            plstypes[i] = 0;
        }
        q = 0;

    // Re-Enable all interrupts
      interrupts();
    /* End RF TX */
    }
}

// Arduino build-in function called by INT when serial data is available (depends board)
void serialEvent() {
  while (Serial.available() and !receive_flag ) {
    // get the new byte
    char c = (char)Serial.read();
    // add it to the inputString
    data[q++] = c;
    // if the incoming character is a @ set receive_flag call receive() in loop() outside ISR
    if(c == '@') {
      data[q++] = '\0';
      receive_flag = true;
      q = 0;
    }
  }
}

void broadcast(uint8_t nrpulses) {
    int i = 0, x = 0, match = 0, p = 0;

    Serial.print("c:");
    for(i=0;i<nrpulses;i++) {
        match = 0;
        for(x=0;x<MAX_PULSE_TYPES;x++) {
            /* We device these numbers by 10 to normalize them a bit */
            if(((plstypes[x]/10)-(codes[i]/10)) <= 2) {

#ifndef SEND_STRIPPED_SPACES 
                /* Every 'space' is followed by a 'pulse'.
                 * All spaces are stripped to spare
                 * resources. The spaces can easily be
                 * added afterwards.
                 */
                if((i%2) == 1) {
                    /* Write numbers */
                    Serial.print(char(48+x));
                }
#else
                /* Write numbers */
                Serial.print(char(48+x));
#endif
                match = 1;
                break;
            }
        }
        if(match == 0) {
            plstypes[p++] = codes[i];

#ifndef SEND_STRIPPED_SPACES 
            /* See above */
            if((i%2) == 1) {
                Serial.print(char(48+p-1));
            }
#else
      Serial.print(char(48+p-1));
#endif 
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
#ifdef ADD_LINE_FEED
    Serial.println('@');
#else
    Serial.print('@');
#endif
}

// Generic ISR function for RF RX pulse interrupt handler
void ISR_RX(){
  // Disable ISR for RF RX interrupt handler only
  detachInterrupt(digitalPinToInterrupt(RX_PIN));

  uint32_t current_counter = micros();
  uint16_t ten_us_counter  = uint16_t((current_counter-new_counter)/10);

  new_counter = current_counter;

    /* We first do some filtering (same as pilight BPF) */
    if(ten_us_counter > MIN_PULSELENGTH) {
        if(ten_us_counter < MAX_PULSELENGTH) {
            /* All codes are buffered */
            codes[nrpulses++] = ten_us_counter;
            if(nrpulses >= BUFFER_SIZE-1) {
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
                        broadcast_flag = nrpulses;
                    }
                    rawlen = nrpulses;
                }
                nrpulses = 0;
            }
        }
    }

  // If no broadcast, re-enable ISR for RF RX interrupt handler
  if (broadcast_flag == 0){
    attachInterrupt(digitalPinToInterrupt(RX_PIN), ISR_RX, CHANGE);
  }
}

void loop(){

  // if receive flag is set
  if (receive_flag){
    // Call to receive()
    receive();
    // Clear flag
    receive_flag = 0;
  }

  // if broadcast flag is set
  if (broadcast_flag > 0){

#ifdef LED_BLINK_RX
    digitalWrite(LED_BLINK_RX, HIGH); // Led blink on RF RX
#endif

    // Call to broadcast()
    broadcast(broadcast_flag);
    // Clear flag
    broadcast_flag = 0;

#ifdef LED_BLINK_RX
    digitalWrite(LED_BLINK_RX, LOW); // Led blink on RF RX
#endif

    // Re-enable ISR for RF RX interrupt handler
    attachInterrupt(digitalPinToInterrupt(RX_PIN), ISR_RX, CHANGE);
  }

#ifdef EVERY_SEC_LINE_FEED
  static unsigned long line_feed_counter = 0;
  if (millis() > line_feed_counter){
    line_feed_counter = millis()+1000;
    Serial.println();
  }
#endif
}
