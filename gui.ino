int x1, y1, w, h, x2, y2;
#define X_OFFSET 18


#define MENU_CHANGE_MHZ 0
#define MENU_CHANGE_KHZ 1
#define MENU_CHANGE_HZ  2
#define MENU_MODE_SWR 3
#define MENU_MODE_PWR 4
#define MENU_MODE_SNA 5
#define MENU_SPAN 6
#define MENU_PLOT 7

#define ACTION_SELECT 1
#define ACTION_DESELECT 2
#define ACTION_UP 3
#define ACTION_DOWN 4

int uiFocus = MENU_CHANGE_MHZ, knob=0, uiSelected = -1;

//
//  IMPROVED ROTARY ENCODER CODE 
//
//  Bruce E. Hall
//  01 June 2019
//
//  Implements rotary encoder with two interrupt service routines:  one for the encoder button
//  and one for the encoder inputs.  These routines are called automatically when the logic
//  state of the encoder pins changes.  You do not have to wait in your code for encoder input.
//  The encoder code requires an additional pin-change interrupt library to function.
//
//  Three flags track the encoder status:
//
//  button_pressed:   true when the button has been pressed.
//  button_released:  true when the button has been released.
//  rotary_changed:   true when encoder position has changed.
//
//  These flags are set the by interrupt routines, and should be reset by your software when
//  you have handled the encoder condition.  For example, if your code does something when
//  button_pressed is true, you will need to set the reset the flag to false.
//
//  Two variables track the encoder value:
//
//  rotary_counter:   a signed integer that indicates the current encoder "position".  The value
//                    increases as you turn the encoder clockwise and decreased when turning the
//                    encoder counter-clockwise.
//  button_downtime:  an unsigned long integer that specifies how long the button was held down
//                    before latest release.  You can use this value in your button_release code to
//                    take different actions for 'short-press' and 'long-press' events.
//
//  To use,  put the following code in your main setup routine:
//
//       pinMode(ENC_A, INPUT_PULLUP);
//       pinMode(ENC_B, INPUT_PULLUP);
//       pinMode(FBUTTON, INPUT_PULLUP);
//       enableInterrupt(ENC_A,rotaryIRQ,CHANGE); 
//       enableInterrupt(FBUTTON,buttonIRQ,CHANGE); 
//
//  And add the following inclues & defines
//  
//      #include <EnableInterrupt.h>                      // https://github.com/GreyGnome/EnableInterrupt
//
//      #define ENC_A                    (A3)             // pin attached to encoder A output
//      #define ENC_B                    (A1)             // pin attached to encoder B output
//      #define FBUTTON                  (A2)             // pin attached to encoder pushbutton
//
//   And the following global variables
//
//      volatile int      rotary_counter      = 0;        // current "position" of rotary encoder (increments CW) 
//      volatile boolean  rotary_changed      = false;    // will turn true if rotary_counter has changed
//      volatile boolean  button_pressed      = false;    // will turn true if the button has been pushed
//      volatile boolean  button_released     = false;    // will turn true if the button has been released (sets button_downtime)
//      volatile uint32_t button_downtime     = 0L;       // ms the button was pushed before released
//


// Rotary Encoder Button Interrupt Service Routine ----------
// Process encoder button presses and releases, including
// debouncing (extra "presses" from noisy switch contacts).
// If button is pressed, the button_pressed flag is set to true.
// If button is released, the button_released flag is set to true,
// and button_downtime will contain the duration of the button
// press in ms.  Manually reset flags after handling event.

void buttonIRQ()
{  
  static boolean button_state = false;
  static boolean pinState;
  static unsigned long start, end;
    
  pinState = digitalRead(FBUTTON);
  if ((pinState == LOW) && (button_state == false))            // Button was up, but is currently down
  {
    start = millis();
    if (start > (end + 10))                                    // 10ms debounce timer
    {
      button_state = true;
      button_pressed = true;
    }
  }
  else if ((pinState == HIGH) && (button_state == true))       // Button was down, but now released
  {
    end = millis();
    if (end > (start + 10))                                    // 10ms debounce timer
    {
      button_state = false;
      button_released = true;
      button_downtime = end - start;
    }
  }
}


// Rotary Encoder Interrupt Service Routine ---------------
// This function will runs when encoder pin A changes state.
// The rotary "position" is held in rotary_counter, 
// increasing for CW rotation, decreasing for CCW rotation.
// If the position changes, rotary_change will be set true. 
// You should set this to false after handling the change.
  
void rotaryIRQ()
{
  static uint8_t rotary_state = 0;                  // holds current and previous encoder states   

  rotary_state <<= 2;                               // shift previous state up 2 bits
  rotary_state |= (digitalRead(ENC_A));             // put enc_A on bit 0
  rotary_state |= (digitalRead(ENC_B) << 1);        // put enc_B on bit 1
  rotary_state &= 0x0F;                             // zero upper 4 bits

  if (rotary_state == 0x09)                         // 9 = binary 1001 = 10 to 01 transition.
  {
    rotary_counter++;
    rotary_changed = true;
  }
  else if (rotary_state == 0x03)                    // 3 = binary 0011 = 00 to 11 transition.
  {
    rotary_counter--;
    rotary_changed = true;
  }
}

boolean buttonDown()                                // check CURRENT state of button
{
  return (digitalRead(FBUTTON)==LOW);
}

void waitForButtonRelease()
{
  if (buttonDown())                                 // make sure button is currently pressed.
  {
    while (!button_released) ;                      // wait for release
    button_released = false;                        // and reset flag
  }
}

void waitForButtonPress()
{
  if (!buttonDown())                                // make sure button is not pressed.
  {
    while (!button_pressed) ;                       // wait for press
    button_pressed = false;                         // and reset flag
  }  
}

// readEncoder returns 0 if no significant encoder movement since last call,
// +1 if clock_wise rotation, and -1 for counter-clockwise rotation
// call with numClicks 0 for highest sensitity, add 1 for each addl click required.

int readEncoder(unsigned int numClicks = 0) {
  static int prevCounter = 0;
  rotary_changed = false;                           // Clear flag
  int change = rotary_counter - prevCounter;        // how many clicks since last call?
  if (abs(change) <= numClicks)                     // not enough clicks?
    return 0;                                       // so exit with a 0.
  prevCounter = rotary_counter;                     // enough clicks, so save current counter values
  return (change>0) ? 1:-1;                         // and return +1 for CW rotation, -1 for CCW rotation    
}


int enc_read() {
  return readEncoder(1);
}

//
//  END OF IMPROVED ROTARY ENCODER CODE 
//

void freqtoa(unsigned long f, char *s){
  int mhz = f/1000000L;
  f -= mhz*1000000L;
  int khz = f/1000;
  int hz = f - khz*1000;
  sprintf(s,"%3d.%03d.%03d",mhz,khz,hz);
}

// updateMeter puts a measurement reading & bar graph on the display
// call with the measurement reading in dB

void updateMeter(int reading){
  int percentage = 0;
  int vswr_reading;
  
  //draw the meter
  GLCD.FillRect(0, 15, 128, 8, WHITE);

  if (mode == MODE_ANTENNA_ANALYZER)
    strcat(c, "  ANT");
  else if (mode == MODE_MEASUREMENT_RX)
    strcat(c, "  MRX");
  else if (mode == MODE_NETWORK_ANALYZER)
    strcat(c, "  SNA");

  if (mode == MODE_ANTENNA_ANALYZER){
    return_loss = openReading(centerFreq) - reading;
    if (return_loss > 30) return_loss = 30;
    if (return_loss < 0) return_loss = 0;
    vswr_reading = pgm_read_word_near(vswr + return_loss);
    sprintf (c, " %d.%01d", vswr_reading/10, vswr_reading%10);
    percentage = vswr_reading - 10;
  }else if (mode == MODE_MEASUREMENT_RX){
    sprintf(c, "%ddbm", reading + dbmOffset);
    percentage = 110 + reading + dbmOffset;
  }
  else if (mode == MODE_NETWORK_ANALYZER) {
    sprintf(c, "%ddbm", reading + dbmOffset);  
    percentage = 110 + reading + dbmOffset;
  }

  GLCD.DrawString(c, 0, 15);  
  //leave the offset to 37 pixels
  GLCD.DrawRoundRect(45, 15, 82, 6, 2);
  GLCD.FillRect(47, 17, (percentage * 8)/10, 2, BLACK); 
}

// this builds up the top line of the display with frequency and mode
void updateHeading() {
  int vswr_reading;
  // tks Jack Purdum W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa(centerFreq, b, DEC);

  if (mode == MODE_ANTENNA_ANALYZER)
    strcpy(c, "SWR ");
  else if (mode == MODE_MEASUREMENT_RX)
    strcpy(c, "PWR ");
  else if (mode == MODE_NETWORK_ANALYZER)
    strcpy(c, "SNA ");

  freqtoa(centerFreq,c);                          // format frequency for display
  GLCD.DrawString(c, 0, 0);                       // and draw it
  itoa(spanFreq/10000, c, 10);                    // convert span to string  
  strcat(c, "K/d");                               // add units
  GLCD.DrawString(c, 128-(strlen(c)*6), 0);       // and right justify it on display
}


void drawCalibrationMenu(int selection){

  GLCD.ClearScreen();
  GLCD.FillRect(0, 0, 128, 64, WHITE);
  GLCD.DrawString("Calibration Mode", 0,0);
  GLCD.DrawString("Return Loss", 20, 20);
  GLCD.DrawString("Freq Calibrate", 20, 40);

  if (selection == 0)
    GLCD.DrawRect(15,15,100,20);
  if (selection == 1)
    GLCD.DrawRect(15,35,100,20);  
}

void calibration_mode(){
  int i, select_freq_cal = 0;

  drawCalibrationMenu(select_freq_cal);

  waitForButtonRelease();
  
  while(!button_pressed){
    i = enc_read();
    
    if(i > 0 && select_freq_cal == 0){
      select_freq_cal = 1;
      drawCalibrationMenu(select_freq_cal);
    }
    else if (i < 0 && select_freq_cal == 1){
      select_freq_cal = 0; 
      drawCalibrationMenu(select_freq_cal);
    }
    delay(50);
  } 
  button_pressed = false; 
 
  if (!select_freq_cal)
    calibrateMeter();
  else
    calibrateClock();

  updateScreen();
}

void saveFrequency() {
  unsigned long last_freq;
  EEPROM.get(LAST_FREQ, last_freq);               // Save current FREQ to EEPROM
  if (last_freq != centerFreq)                    // but only if it changed
    EEPROM.put(LAST_FREQ, centerFreq); 
}

void uiFreq(int action){

  GLCD.FillRect(0, 25, 128, 11, WHITE);  
  freqtoa(centerFreq, b);
  GLCD.DrawString(b, 39, 27);
  if (uiFocus == MENU_CHANGE_MHZ)
    GLCD.DrawRect(38,25,18,11);
  else if (uiFocus == MENU_CHANGE_KHZ)
    GLCD.DrawRect(62,25,18,11);
  else if (uiFocus == MENU_CHANGE_HZ)
    GLCD.DrawRect(86,25,18,11);

  if (!action) return; 

  if (action == ACTION_SELECT) {
    if (uiFocus == MENU_CHANGE_MHZ)
      GLCD.InvertRect(38,25,18,11);
    else if (uiFocus == MENU_CHANGE_KHZ)
      GLCD.InvertRect(62,25,18,11);
    else if (uiFocus == MENU_CHANGE_HZ)
      GLCD.InvertRect(86,25,18,11);

    waitForButtonRelease();
     
    while(!button_pressed){                               // stay in this loop until button press
      int i = readEncoder(0);                             // check for encoder rotation
      if (i==0) continue;                                 // nothing yet, so just wait    
      if (uiFocus == MENU_CHANGE_MHZ)                     // get here = frequency changed
        centerFreq += 1000000l * i;                       // change MHz, up or down
      else if (uiFocus == MENU_CHANGE_KHZ)                // change kHz, up or down
        centerFreq += 1000l * i;
      else if (uiFocus == MENU_CHANGE_HZ)                 // change Hz, up or down
        centerFreq += i;
      if (centerFreq > MAX_FREQ)                          // cannot go below minimum freqency
        centerFreq = MAX_FREQ;
      if (centerFreq < MIN_FREQ)                          // cannot go above maximum frequency
        centerFreq = MIN_FREQ;
      setOscillators(centerFreq);                         // go to specified frequency
      takeReading();                                      // take reading & update meter

      GLCD.FillRect(0, 25, 128, 11, WHITE);               // erase previous frequency display
      freqtoa(centerFreq, b);                             // convert new value into a string
      GLCD.DrawString(b, 39, 27);                         // and display it
 
      if (uiFocus == MENU_CHANGE_MHZ)                     // re-invert selection
        GLCD.InvertRect(38,25,18,11);
      else if (uiFocus == MENU_CHANGE_KHZ)
        GLCD.InvertRect(62,25,18,11);
      else if (uiFocus == MENU_CHANGE_HZ)
        GLCD.InvertRect(86,25,18,11);
    }
    button_pressed = false;                               // button finally pressed, handle it here:

    GLCD.FillRect(0, 25, 128, 11, WHITE);                 // erase displayed frequency
    freqtoa(centerFreq, b);                               // convert new freq to a string
    GLCD.DrawString(b, 39, 27);                           // and display it!
    if (uiFocus == MENU_CHANGE_MHZ)                       // redraw surrounding selection box
      GLCD.DrawRect(38,25,18,11);
    else if (uiFocus == MENU_CHANGE_KHZ)
      GLCD.DrawRect(62,25,18,11);
    else if (uiFocus == MENU_CHANGE_HZ)
      GLCD.DrawRect(86,25,18,11);
    saveFrequency();                                      // save current frequency to EEPROM
  }  
}

void uiSWR(int action){
  GLCD.FillRect(7,38,20,10, WHITE);
  GLCD.DrawString("SWR", 9, 40);

  if (action == ACTION_SELECT){
    mode = MODE_ANTENNA_ANALYZER;
    uiPWR(0);
    uiSNA(0);
    updateScreen();
    EEPROM.put(LAST_MODE, mode);
  }
  
  if (uiFocus == MENU_MODE_SWR)
    GLCD.DrawRect(7,38,20,10);

  if (mode == MODE_ANTENNA_ANALYZER)
    GLCD.InvertRect(8,39,18,8);    

  setOscillators(centerFreq);
  updateMeter(readDB());
}

void uiPWR(int action){
  GLCD.FillRect(31,38,20,10, WHITE);    
  GLCD.DrawString("PWR", 33, 40);

  if (action == ACTION_SELECT){
    mode = MODE_MEASUREMENT_RX;
    uiSWR(0);
    uiSNA(0);
    updateScreen();
    EEPROM.put(LAST_MODE, mode);
  }
  if (uiFocus == MENU_MODE_PWR)
    GLCD.DrawRect(31,38,20,10);
  if (mode == MODE_MEASUREMENT_RX)
    GLCD.InvertRect(32,39,18,8);
  
  setOscillators(centerFreq);
  updateMeter(readDB());
}


void uiSNA(int action){
  GLCD.FillRect(55,38,20,10, WHITE);
  GLCD.DrawString("SNA", 57, 40);

  if (action == ACTION_SELECT){
    mode = MODE_NETWORK_ANALYZER;
    uiSWR(0);
    uiPWR(0);
    updateScreen();
    EEPROM.put(LAST_MODE, mode);
  }
  if (uiFocus == MENU_MODE_SNA)
    GLCD.DrawRect(55,38,20,10);
  if (mode == MODE_NETWORK_ANALYZER)
    GLCD.InvertRect(56,39,18,8);
  
  setOscillators(centerFreq);
  updateMeter(readDB());
}

void uiSpan(int action){
  GLCD.FillRect(55, 49, 24, 12, WHITE);
  if (spanFreq >= 1000000l)
    sprintf(b, "SPAN +/-%3ldM", spanFreq/1000000l);
  else
    sprintf(b, "SPAN +/-%3ldK", spanFreq/1000l);
    
  GLCD.DrawString(b, 6,52);
  if (uiFocus == MENU_SPAN)
    GLCD.DrawRect(55, 50, 24, 10);

  if (action == ACTION_SELECT) {
   GLCD.InvertRect(55, 51, 24, 8);                       //invert the selection    
   while(!button_pressed){
     if (rotary_changed)
     {
        rotary_changed = false;                         // handle encoder movement here
        int i = readEncoder(2);
        if (i !=0)
        {
          if (selectedSpan > 0 && i < 0){
            selectedSpan--;
            spanFreq = spans[selectedSpan];
            EEPROM.put(LAST_SPAN, selectedSpan);
            delay(200);
          }
          else if (selectedSpan < MAX_SPANS && i > 0){
            selectedSpan++;
            spanFreq = spans[selectedSpan];
            EEPROM.put(LAST_SPAN, selectedSpan);
            delay(200);
          }
          GLCD.FillRect(55, 49, 24, 10, WHITE);
          if (spanFreq >= 1000000l)
            sprintf(b, "SPAN +/-%3ldM", spanFreq/1000000l);
          else
            sprintf(b, "SPAN +/-%3ldK", spanFreq/1000l);
          GLCD.DrawString(b, 6,52);
          GLCD.InvertRect(55, 51, 24, 8);
       }  // if i!=0
      }  // if rotary changed    
    } //while
    button_pressed = false;                             // handle press by exiting
    
  } // action_select
}

void uiPlot(int action){
  GLCD.FillRect(90, 42, 37,20, WHITE);
  GLCD.DrawRect(90, 42, 37,20);
  GLCD.DrawString("PLOT", 98, 49);

  if (uiFocus == MENU_PLOT)
    GLCD.DrawRect(92, 44, 33, 16);

  if (action == ACTION_SELECT){
    if (mode == MODE_ANTENNA_ANALYZER)
      setupVSWRGrid();
    else 
      plotPower();
    updateScreen();
  }
}


void uiMessage(int id, int action){
  switch(id){
    case MENU_CHANGE_MHZ:
    case MENU_CHANGE_KHZ:
    case MENU_CHANGE_HZ:
      uiFreq(action);
      break;
    case MENU_MODE_SWR:
      uiSWR(action);
      break;
    case MENU_MODE_PWR:
      uiPWR(action);
      break;
    case MENU_MODE_SNA:
      uiSNA(action);
      break;
    case MENU_SPAN:
      uiSpan(action);
      break;
    case MENU_PLOT:
      uiPlot(action);
      break;
    default:
      return;
  }
}

void updateScreen(){
  strcpy(b, "#Antuino - ");                          
  GLCD.ClearScreen();
  switch (mode){                                   // show mode on title bar
    case MODE_ANTENNA_ANALYZER:
      strcat(b, "SWR");
      break;
    case MODE_MEASUREMENT_RX:
      strcat(b, "PWR");
      break;
    case MODE_NETWORK_ANALYZER:
     strcat(b, "SNA");
     break;
  }
  GLCD.DrawString(b, 1, 1);                        // draw the title bar
  GLCD.InvertRect(0,0, 128,9);                     // and invert it

  //update all the elements in the display
  updateMeter(readDB());
  uiFreq(0);
  uiSWR(0);
  uiPWR(0);
  uiSNA(0);
  uiSpan(0);
  uiPlot(0);
}

void advanceFocus (boolean clockwise = true)
{
  int prevFocus = uiFocus;                        // save current focus
  if (clockwise) {                                // CW rotation     
    uiFocus++;                                    // focus on next menu item
    if (uiFocus>7) uiFocus=0;                     // stay on menu!
  } else {                                        // CCW rotation
    uiFocus--;                                    // go to previous menu item
    if (uiFocus<0) uiFocus=7;                     // stay on menu
  }
  uiMessage(prevFocus,0);                         // defocus prior menu item
  uiMessage(uiFocus,0);                           // focus on new menu item
}


void doMenu(){
  if (rotary_changed)                             // check for encoder movement
  {
    rotary_changed = false;                       // handling it here.
    int encoderMovement = readEncoder(1);         // see if it moved enough
    if (encoderMovement>0) advanceFocus(true);    // +1 = clockwise movement
    if (encoderMovement<0) advanceFocus(false);   // -1 = counter-clockwise movement
  }
  if (button_pressed)                             // check for button press
  {
    button_pressed = false;                       // handling it here
    uiMessage(uiFocus, ACTION_SELECT);            // menu item called to action!
    advanceFocus(true);                           // deselect & go to next field
  } 
}
