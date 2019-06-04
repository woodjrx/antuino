/**************************************************************************

 ANTUINO: An antenna-analyzer & compact RF-lab from HF SIGNALS
 
 Created by:    Ashar Farhan
 Modified by:   Bruce E. Hall, W8BH
 Last mod:      01 Jun 2019
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
#define MIN_FREQ                 10000l           // 10 kHz
#define MAX_FREQ                 150000000l       // 150 MHz

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
 
unsigned long centerFreq = 14000000l;             // 14 MHz
unsigned long spanFreq   = 20000000l;             // 20 MHz

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


int mode                       = MODE_ANTENNA_ANALYZER;
int selectedSpan               = 0;
uint32_t xtal_freq_calibrated  = 27000000l;
char b[32],c[32];                               // temporary character arrays
int return_loss;
int openHF                     = 96;
int openVHF                    = 96;
int openUHF                    = 68;
int dbmOffset                  = -114;


int readDB() {
  return analogRead(DBM_READING)/5;                // return #dB steps from AD8307
                                                   // will require offset to get actual dBm
}

void active_delay(int delay_by){
  unsigned long timeStart = millis();

  while (millis() - timeStart <= delay_by) {
      //Background Work      
  }
}

void updateDisplay(){
  sprintf(b, "%ldK, %ldK/div", centerFreq/1000, spanFreq/10000); 
  GLCD.DrawString(b, 20, 57);
}

int calibrateClock(){
  int knob = 0;
  //int32_t prev_calibration;

  GLCD.ClearScreen();
  GLCD.DrawString("1. Monitor Antenna", 0, 0);
  GLCD.DrawString("  port on 10 MHz freq.", 0, 10);
  GLCD.DrawString("2. Tune to zerbeat and", 0, 20);
  GLCD.DrawString("3. Click to Save", 0, 30);

  GLCD.DrawString("Save", 64, 45);
  GLCD.DrawRect(60,40,35,20);

  waitForButtonRelease();

  //prev_calibration = xtal_freq_calibrated;
  xtal_freq_calibrated = 27000000l;

  si5351aSetFrequency_clk1(10000000l);  
  ltoa(xtal_freq_calibrated - 27000000l, c, 10);
  GLCD.FillRect(0,40,50,15, WHITE);
  GLCD.DrawString(c, 4, 45);     

  while (!button_pressed)
  {
    knob = enc_read();

    if (knob > 0)
      xtal_freq_calibrated += 10;
    else if (knob < 0)
      xtal_freq_calibrated -= 10;
    else 
      continue; //don't update the frequency or the display

    si5351aSetFrequency_clk1(10000000l);  
      
    ltoa(xtal_freq_calibrated - 27000000l, c, 10);
    GLCD.FillRect(0,40,50,15, WHITE);
    GLCD.DrawString(c, 4, 45);     
  }

  waitForButtonRelease();
  GLCD.ClearScreen();
  GLCD.DrawString("Calibration Saved", 0, 25);

  EEPROM.put(MASTER_CAL, xtal_freq_calibrated);
  delay(2000);
}

int readOpen(unsigned long f){
  int i, r;
  
  setOscillators(f);                                       // set osc outputs according to  mode
  delay(100);                                              // allow time for outputs to settle
  r = 0;
  for (i = 0; i < 10; i++){                                // take ten measurements
    r += readDB();
    delay(50);
  }
  delay(1000);                                             // allow time between calls
  return r/10;                                             // return average of 10 measurements
}

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
  if (prevFreq != freq || prevMode != mode){                           // freq or mode changed
  
    if (freq < 20000l) freq = 20000l;
    if (freq < 150000000l)
    {
      if (freq < 50000000l)
        local_osc = freq + IF_FREQ;
      else
        local_osc = freq - IF_FREQ;
    } else {
      freq = freq / 3;
      local_osc = freq - IF_FREQ/3;
    }
 
    switch(mode){
    case MODE_MEASUREMENT_RX:
      si5351aSetFrequency_clk2(local_osc);
      si5351aOutputOff(SI_CLK1_CONTROL);
      si5351aOutputOff(SI_CLK0_CONTROL);
    break;
    case MODE_NETWORK_ANALYZER:
      si5351aSetFrequency_clk2(local_osc);
      si5351aOutputOff(SI_CLK1_CONTROL);        
      si5351aSetFrequency_clk0(freq);
    break;
    default:  // MODE_ANTENNA_ANALYZER
      si5351aSetFrequency_clk2(local_osc);  
      si5351aSetFrequency_clk1(freq);
      si5351aOutputOff(SI_CLK0_CONTROL);        
    }      
    prevFreq = freq;
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

void loop()
{
  doMenu();                            // handle user input
  takeReading();                       // measure & display result @ displayed frequency 
  delay(40);                           // put some time between analog reads
}
