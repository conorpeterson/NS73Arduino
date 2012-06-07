 Revision 1 (5/7/2012).
 Arduino driver for the Niigata Seimitsu NS73M low-power FM transmitter.
 Copyright (C) 2012 Conor Peterson (conor.p.peterson@gmail.com)

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
 
  
 In addition to basic frequency selection, this driver implements most of the other interesting features of the NS73M
 including selectable transmission power, adjustable input sensitivity, muting, and the ability to turn the broadcast
 on and off to save power. At present it uses SPI to communicate (three-wire serial, i.e. data clock and latch).
 My best guess as to SRAM usage is about 32 bytes. It also needs around 30 bytes of EEPROM space to store
 calibration information.
 
 Basic use:
 0. Copy NS73.cpp and NS73.h into your sketch folder. Make sure to #include "NS73.h" at the top of your sketch.
 1. Call begin() with the pins the NS73 is attached to.
 2. Call setChannel() or setFrequency().
 3. Call goOnline() to bring the transmitter online.
 4. Adjust channel as you see fit. You do not need to take the transmitter offline to change channels.
 
 Advanced use notes:
 - You can use setFrequency() to directly set the frequency without having to look up the channel number.
 The argument is an int. setFrequency(875) = 87.5 MHz. setFrequency(1015) = 101.5 MHz etc.
 If you try to set it between channels, it will round down to the nearest channel. 
 - getFrequency() is similar. It returns an int. You can use this to help drive an LCD display.
 You have to figure out the decimal place on your own. ;)
 - The NS73 does need some calibration in order to lock onto certain frequencies. The built-in calibration is
 pretty good and has worked for many chips. If it fails to get a frequency lock when changing channels, the
 driver will attempt to adjust the oscillator. This might take two full seconds! If it succeeds, it will
 modify the calibration table and future channel changes will happen much faster.

 CONNECTING TO YOUR ARDUINO:
 To use an NS73 breakout board from Sparkfun in your Arduino project, connect its power to the arduino's 3.3v
 power rail, connect the gnds, and tie the IIC pin to gnd. The arduino communicates at +5V, which will potentially
 cook your NS73. DO NOT CONNECT THE I/O LINES TOGETHER!
 
 To avoid this you MUST use special circuitry on the data, latch and clock lines. (TEB is OK as is).
 I prefer to do this with a 3.3v Zener diode (1N5226B) and a 10kΩ resistor. Repeat this circuit for each of the
 i/o lines.

 The following diagram is going to suck when viewed in HTML -- download the code and look at this in your text editor.
 
  Out (+5) from Arduino O------Z<|---+---/\/\/\---GND
                                 |
                                 O In (~3.15v) to the NS73.

 There are other more and less robust ways to accomplish this. Note the unidirectional i/o. The NS73 never needs
 to talk back to the Ardunio which simplifies matters.
 
 ABOUT CEX:
 The NS73 obtains its VHF carrier signal by implementing a small frequency synthesizer. The input is a 32.768 KHz
 crystal (supplied on the breakout board) which is then divided down to 8.192KHz. It then uses a clock multiplied
 phase-locked loop to produce signals in the VHF band. The PLL has a couple of adjustment registers. The first
 are the charge pumps. My guess is that these control the power available to the PLL to correct itself when the
 output frequency needs to change. Typically you bring the main synthesizer charge pump to 80µA when changing
 frequency and drop it back to the lower setting after it stabilizes. The status of the PLL is expressed through
 the TEB line: if TEB is high, the synthesizer has a good lock.
 
 The second adjustment is CEX, or Oscillator Extension. I do not know exactly what the underlying mechanism is,
 but it acts as a coarse adjustment for the synthesizer. It has four settings. Higher frequencies use lower bands
 and vice versa. If the CEX setting is wrong for any given frequency, the synthesizer will fail to get a lock.
 The driver detects this condition and attempts to correct for it. If it successfully recovers the new setting
 is committed to EEPROM.
 
 If you wish to force a total recalibration of your NS73, modify the default CEX bands to junk settings (all bands
 start at channel 0, for example), clear the arduino's EEPROM, and force the chip to try all channels in sequence.
 Sometimes it takes multiple passes to get a good calibration for every channel. You'll know that calibration is
 complete if you can hit every channel without ever losing TEB lock.

 Hat tip to Lee Montgomery of Neighborhood Public Radio -- http://neighborhoodpublicradio.org
