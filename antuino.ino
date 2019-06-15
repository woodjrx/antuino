/**************************************************************************

 ANTUINO: An antenna-analyzer & compact RF-lab from HF SIGNALS
 
 Created by:    Ashar Farhan
 Modified by:   Bruce E. Hall, W8BH
 Last mod:      04 Jun 2019
 Hardware:      Antuino board with embedded Arduino Nano
 Environment:   Arduino IDE 1.8.9

 This is a fork of the Antuino project by AFarhan at:
 https://github.com/afarhan/antuino
 Also see his website at:  http://www.hfsignals.com/index.php/antuino/
 
 **************************************************************************/



#include <glcd.h>                                 // https://github.com/woodjrx  
#include <fonts/allFonts.h>
#include <Wire.h>
#include <EEPROM.h>
#include <EnableInterrupt.h>                      // https://github.com/GreyGnome/EnableInterrupt

/* offsets into the EEPROM storage for calibration */
#define MASTER_CAL               0
#define LAST_FREQ                4
#define OPEN_HF                  8
#define OPEN_VHF                 12
#define OPEN_UHF                 16
#define LAST_SPAN                20
#define LAST_MODE                24

//Register definitions to switch on/off various clocks
#define SI_CLK0_CONTROL          16      
#define SI_CLK1_CONTROL          17
#define SI_CLK2_CONTROL          18

#define IF_FREQ                  (24996000l)
#define MODE_ANTENNA_ANALYZER    0
#define MODE_MEASUREMENT_RX      1
#define MODE_NETWORK_ANALYZER    2

// The following defines map to I/O pins on the embedded arduino NANO
#define DBM_READING              (A6)
#define ENC_A                    (A3)             // pin attached to encoder A output
#define ENC_B                    (A1)             // pin attached to encoder B output
#define FBUTTON                  (A2)             // pin attached to encoder pushbutton

// The folowing defines set the frequency range of the device, in Hz
#define MIN_FREQ                 100000L          // 100 kHz
#define MAX_FREQ                 150000000L       // 150 MHz

// Global variables that can be changed by the encoder interrupt routines

volatile int      rotary_counter      = 0;        // current "position" of rotary encoder (increments CW) 
volatile boolean  rotary_changed      = false;    // will turn true if rotary_counter has changed
volatile boolean  button_pressed      = false;    // will turn true if the button has been pushed
volatile boolean  button_released     = false;    // will turn true if the button has been released (sets button_downtime)
volatile uint32_t button_downtime     = 0L;       // ms the button was pushed before released

// The following two variables define which frequencies are plotted.
// The center frequency is the number displayed on the main screen,
// and represents the middle frequency that will be plotted.
// The span is the range of frequencies to be plotted.
// For example, a freq of 14 MHz and span of 20 MHz will plot
// from 14-(20/2)= 4 Mhz to 15+(20/2)= 25 MHz.
 
long centerFreq = 14000000L;                      // 14 MHz
long spanFreq   = 20000000L;                      // 20 MHz

// The following variables define the spans.  
// spans[] is a list of possible span choices, 25Mhz to 5 KHz

#define MAX_SPANS 12
long spans[] = {
             20000000l, 10000000l,               //  10 MHz spans
   5000000l,  2000000l,  1000000l,               //   1 MHz spans
    500000l,   2000001,   100000l,               // 100 kHz spans
     50000l,    20000l,    10000l,               //  10 kHz spans
      5000l                                      //   1 kHz spans 
};

// the following table converts Return Loss numbers into SWR. 
// The table includes 30 entries, from RL=0 dB to 29 dB.
// The entry = 10 * VSWR.  For example, notice that the second
// entry (index 1) is 174.   RL of 1 dB = VSWR of 17.4.
// The table is stored in program memory to save space for variables
 
const int PROGMEM vswr[] = {
999, 174, 87,  58,  44,  35,  30,  26,  23,  21,  19,  18,  17,
16,  15,  14,  14,  13,  13,  12,  12,  12,  12,  11,  11,  11,
11,  11,  10,  10,  10 
};

const int PROGMEM lut[] = {                              // analog reading @ each attenuation level
  404, 402, 400, 398, 396, 395, 393, 391, 388, 386,      //  0- 9 dB attenuation
  384, 383, 380, 376, 374, 369, 366, 363, 359, 356,      // 10-19 dB
  352, 349, 345, 341, 338, 334, 329, 325, 320, 315,      // 20-29 dB
  310, 305, 300, 294, 288, 282, 276, 269, 263, 257,      // 30-39 dB
  251, 245, 238, 232, 225, 218, 211, 205, 198, 192,      // 40-49 dB
  186, 181, 176, 170, 165, 161, 156, 151, 148, 144,      // 50-59 dB
  140, 137, 134, 130, 128, 125, 123, 121, 119, 118,      // 60-69 dB
  116
};

int mode                       = MODE_ANTENNA_ANALYZER;
int selectedSpan               = 0;
uint32_t xtal_freq_calibrated  = 27000000L;
char b[32],c[32];                                        // temporary character arrays
int return_loss;
int openHF                     = 96;
int openVHF                    = 96;
int openUHF                    = 68;
int dbmOffset                  = -114;
int dbmOutputPort              = 0;                      // output port power

int readDbm() {
  int raw = analogRead(DBM_READING);                     // get analog input from AD8307
  int i=0;                                               // start with dB attenuation of 0.
  while((i<70)&&(pgm_read_word_near(lut + i)>raw))       // search the lookup table...
    i++;                                                 // and find the analog value, so index = dB
  return (dbmOutputPort-i);                              // dBm =  output port power - attenuation 
}

int readDB() {
return analogRead(DBM_READING)/5;                        // return #dB steps from AD8307
                                                         // will require offset to get actual dBm
}

void updateDisplay(){
  sprintf(b, "%ldK, %ldK/div", centerFreq/1000, spanFreq/10000); 
  GLCD.DrawString(b, 20, 57);
}



int readOpen(unsigned long f){
   
  setOscillators(f);                                       // set osc outputs according to  mode
  delay(100);                                              // allow time for outputs to settle
  int r = 0;
  for (int i = 0; i < 10; i++){                            // take ten measurements
    r += readDB();
    delay(50);
  }
  delay(1000);                                             // allow time between calls
  return r/10;                                             // return average of 10 measurements
}


// Calibration works by injecting a signal of known power (-19.5 dBm) onto the receiver (input port), 
// measuring the voltage that signal creates, and calculating an offset that will turn this result into
// -19.5 dBm.   Mathematically, reading + offset = -19.5 dBm; rearranged: offset = -19.5 - reading.  
// For example, if the measured dB reading at 20 MHz (openHF) is 74, the dB offset is
// calculated as -19.5 - 74 = -93.5.  Now, if an incoming signal measures 35, its calculated power is
// 35 - 93.5 = -58.5 dBm.

int calibrateMeter(){
  
  GLCD.ClearScreen();
  GLCD.DrawString("Disconnect Antenna", 0, 0);             // give user instructions
  GLCD.DrawString("port and press Button", 0, 10);
  GLCD.DrawString("to calibrate SWR", 0, 20);
  GLCD.DrawString("OK", 10, 42);
  GLCD.DrawRect(5,35,20,20);
    
  waitForButtonPress();                                    // wait for a button down

  GLCD.ClearScreen();
  GLCD.DrawString("Calibrating.....", 10, 25);
  delay(1000);
  
  int r;
  mode = MODE_ANTENNA_ANALYZER;
  delay(100);
  r = readOpen(20000000l);                                 // get HF open reading
  Serial.print(F("HF open reading: ")); 
  Serial.println(r);
  EEPROM.put(OPEN_HF, r);                                  // and save to EEPROM

  r = readOpen(140000000l);
  Serial.print(F("VHF open reading: "));                   // get VHF open reading
  Serial.println(r);
  EEPROM.put(OPEN_VHF, r);

  r = readOpen(440000000l);
  Serial.print(F("UHF open reading: "));                   // get UHF open reading
  Serial.println(r);
  EEPROM.put(OPEN_UHF, r);                                 // and save to EEPROM
 
  GLCD.ClearScreen();
  GLCD.DrawString("Done!",10,25);
  delay(1000);
}

int openReading(unsigned long f){                          // returns saved open Reading value
  if (f < 60000000l)
    return openHF;
  else if (f < 150000000l)
    return openVHF;
  else
    return openUHF;
}


void setOscillators (long freq){
  static long prevFreq = 0;
  static int prevMode = 0;
  long local_osc;
  if (prevFreq != freq || prevMode != mode){                        // the freq or mode changed
                                                                    // so update oscillators....
    if (freq < MIN_FREQ) freq = MIN_FREQ;
    if (freq < 150000000l)                                          // for VHF and HF:
    {
      if (freq < 50000000l)                                         // calculate mixer input
        local_osc = freq + IF_FREQ;                                 // <50MHz: high-side mixing
      else
        local_osc = freq - IF_FREQ;                                 // >50MHz: low-side mixing
    } else {
      freq = freq / 3;                                              // for UHF, use 3rd harmonic
      local_osc = freq - IF_FREQ/3;
    }
 
    switch(mode){
    case MODE_MEASUREMENT_RX:                                       // for power measurements:
      si5351aSetFrequency_clk2(local_osc);                          // you just need receiver
      si5351aOutputOff(SI_CLK1_CONTROL);                            // (antenna output off)
      si5351aOutputOff(SI_CLK0_CONTROL);                            // (output port off)
    break;
    case MODE_NETWORK_ANALYZER:                                     // for network analyzer:
      si5351aSetFrequency_clk2(local_osc);                          // tune receiver to freq
      si5351aOutputOff(SI_CLK1_CONTROL);                            // (antanna output off)
      si5351aSetFrequency_clk0(freq);                               // and transmit on output port
    break;
    default:  // MODE_ANTENNA_ANALYZER                              // for antenna analyzer:
      si5351aSetFrequency_clk2(local_osc);                          // tune receiver to freq
      si5351aSetFrequency_clk1(freq);                               // and inject signal on ant port
      si5351aOutputOff(SI_CLK0_CONTROL);                            // (output port off)
    }      
    prevFreq = freq;                                                // save new freq & mode
    prevMode = mode;
  }     
}  


void setup() {
  pinMode(ENC_A, INPUT_PULLUP);                       // set up rotary encoder pins
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);
  enableInterrupt(ENC_A,rotaryIRQ,CHANGE);            // rotary encoder interrupts
  enableInterrupt(FBUTTON,buttonIRQ,CHANGE); 
  
  GLCD.Init();                                        // initialize LCD screen
  GLCD.SelectFont(System5x7);
  
  b[0]= 0;

  Wire.begin();                                      // init I2C comm with oscillator
  Serial.begin(9600);                                // init serial port output
  Serial.flush();
  Serial.println(F("*Antuino v1.2"));
  analogReference(DEFAULT);

  unsigned long last_freq = 0;
  EEPROM.get(MASTER_CAL, xtal_freq_calibrated);      // get all stored values from EEPROM
  EEPROM.get(LAST_FREQ, last_freq);
  EEPROM.get(OPEN_HF, openHF);
  EEPROM.get(OPEN_VHF, openVHF);
  EEPROM.get(OPEN_UHF, openUHF);
  EEPROM.get(LAST_SPAN, selectedSpan);
  EEPROM.get(LAST_MODE, mode);

  //the openHF reading is actually -19.5 dbm
  dbmOffset = -19.5 - openHF;

  if (last_freq > MIN_FREQ && last_freq < MAX_FREQ)
      centerFreq = last_freq;                         // enforce valid frequency

  if (xtal_freq_calibrated < 26900000l || xtal_freq_calibrated > 27100000l)
    xtal_freq_calibrated = 27000000l;

  if (mode < 0 || mode > 2)  mode = 0;                // enforce valid mode
  spanFreq = spans[selectedSpan];                     // set span according to saved value
  setOscillators(centerFreq);                         // set oscillators to requested frequency
  updateScreen();                                     // take measurement & show main screen
  Serial.print(F("Center Frequency (Hz): "));         // print out current frequency
  Serial.println(centerFreq,DEC);
  Serial.print(F("openHF value: "));
  Serial.println(openHF);
  if (buttonDown()) calibration_mode();               // do CAL if button down on startup
}

void takeReading()
{
  static int prevReading = 0;
  int newReading = readDB();
  if (newReading != prevReading){
    updateMeter(newReading);
    prevReading = newReading;
  }  
}

// debugReading(): prints AD8307 output to serial port as dBm reading & voltage 

void debugReading() {
  const int WAIT_TIME = 10000;                // time between readings, in msec
  static long lastCheck = 0;
  char output[70];                            // buffer for output string
  if (millis() > (lastCheck + WAIT_TIME)) {   // have we waiting long enough yet?
    lastCheck = millis();                     // yes, update wait counter
    int raw = analogRead(DBM_READING);        // read voltage from AD8307 output
    float volts = (raw/1024.0) * 5;           // ..convert it into voltage
    int corrected = readDbm();                // try new lookup table correction
    sprintf(output,                           // format result into a string
      "Reading %d dBm (corrected %d dBm).   Raw Data: %3d (%d.%d volts)", 
       (raw/5)+dbmOffset,  corrected, raw, int(volts), int(volts*100) % 100);
    Serial.println(output);                   // send result to serial port
  }
}

void loop()
{
  doMenu();                            // handle user input
  takeReading();                       // measure & display result @ displayed frequency 
  delay(40);                           // put some time between analog reads
  debugReading();                      // periodically write reading to serial port
}
