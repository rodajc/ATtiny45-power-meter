Description:
Measure current with a split-coil current transformer SCT013-000 clamped onto an electric cable.
The secondary coil of the current transformer is connected to a resistor to generate a voltage.
This voltage can now be measured with the bipolar differential ADC of the attiny45 microcontroller.
This is done by taking samples during approx. 200 ms i.e. 10 waves @ 50 Hz.
The sum of the results is divided by the number of samples to achieve the average voltage.
The average is proportional to the current in the electric cable going through the SCT013-000.
The root mean square can be calculated as follows:
Vrms = Vp / sqrt(2)
Vmean = Vp * 2/π
Vp = Vrms * sqrt(2)
Vmean = Vrms * sqrt(2) * 2/π
Vrms = Vmean * π / 2 / sqrt(2) = approx. 1.11 * Vmean
To calculate the current in the primary coil of the current transformer from the average voltage measured
by the attiny45 the load resistor of the SCT013-000 and the reference voltage come into play.
It may be easier to follow a different approach. Just increase the current in the primary coil switching on
a heavy known load, like a boiler, and write down the increase that the attiny45 measures. Dividing both values
gives the multiplier to be used for all measurements.
The load resistor of the SCT013-000 depends on the maximum current to be measured. Here a value of 47 ohms is
used. The consumed power is limited to 4.4 kW, but this may be exceeded temporarily. A safe margin would be
a maximum power of 6.9 kW, which is 30 A @ 230 V. 30 A RMS corresponds to a peak current of 42.4 A.
This means a peak current of 50 * 42.4 / 100 = 21.2 mA.
The voltage in the secondary coil is limited to 1V, so the load resistor = 1 / 21.2 = 47 ohms.

Communication with the attiny45 is done through USB, using USB control messages.
The command line tool tinysct can be compiled in the commandline folder.
There are two Makefiles, one for PC and for Openwrt.

Command line tool usage:
  tinysct testcomm
    Tests the USB communication with the device. Should give "communication test succeeded". Only for debugging purposes.
  tinysct getosccal
    Retrieves the current OSCCAL value used by the device to calibrate its internal HF PLL. Only for debugging purposes.
  tinysct runadc
    Start ADC sampling during 200 ms
  tinysct getacc
    Get accumulative result of ADC.
  tinysct getcnt
    Get number of ADC samples performed during 200 ms.
  tinysct getadc
    Get average ADC result

Trick1: Allow 200 ms for the measurement to complete after starting it with tinysct runadc. Otherwise the result will be 0.
Trick2: Instead of using average ADC result, divide accumulative result and number of samples to obtain higher resolution.


Fuse bytes:

# Fuse high byte:
# 0xdf = 1 1 0 1   1 1 1 1
#        ^ ^ ^ ^   ^ \-+-/ 
#        | | | |   |   +------ BODLEVEL 2..0 (Brown-Out Detection -> disabled)
#        | | | |   +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved)
#        | | | +-------------- WDTON (watchdog timer always on -> disabled)
#        | | +---------------- SPIEN (enable serial programming -> enabled)
#        | +------------------ DWEN (debug wire enable -> disabled)
#        +-------------------- RSTDISBL (disable external reset -> enabled)
#
# Fuse low byte:
# 0xe1 = 1 1 1 0   0 0 0 1
#        ^ ^ \+/   \--+--/
#        | |  |       +------- CKSEL 3..0 (clock selection -> HF PLL)
#        | |  +--------------- SUT 1..0 (start up time -> slowly rising power)
#        | +------------------ CKOUT (clock output on CKOUT pin -> disabled)
#        +-------------------- CKDIV8 (divide clock by 8 -> don't divide)

