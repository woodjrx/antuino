# Installation

1. To install, add the appropriate library(ies):
   - openGLCD https://bitbucket.org/bperrybap/openglcd/wiki/Home
   - Enable Interrupt https://github.com/GreyGnome/EnableInterrupt

2. And then move the following file:
   - ks0108_Arduino.h  -->  ARDUINO_LIBRARY_FOLDER/openGLCD/config/ks0108/ks0108_Arduino.h

3. You need to then enable this particular configuration by disabling any enabled openGLCD configurations in 'ARDUINO_LIBRARY_FOLDER/openGLCD/openGLCD_Config.h', and then appending the following lines somewhere before the final #endif:
   - '''
     // Antuino specific configuration
     #define GLCDCFG_GLCDCONFIG "config/ks0108/ks0108_Arduino.h"
     '''

The reason for this, per Farhan:
>We had to reconfigure how the GLCD is connected to the Arduino. So it is different from the that of the standard GLCD library's pin configuration. Our custom configuration of the GLCD pin connection is defined in a file called ks0108_Arduino.h

4.  Remember to recalibrate both Power and Frequency after flashing!  (Hold down encoder while turning on to enable)

# Notes
- Moved to openGLCD as the original GLCD has been deprecated for some time.
- Moved to bhall66's antuino.ino to enable significantly improved rotary encoder, and address linearity.
