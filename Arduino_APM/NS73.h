/*
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
 (0. Copy NS73.cpp and NS73.h into your sketch folder. Make sure to #include "NS73.h" at the top of your sketch.)
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
 */ 

//TODO: Always const in function declarations if possible
//TODO: Convert global consts to static members of NS73Class.
//TODO: Reassess what should be public and what should be private
//TODO: Refactor everything declared static const into uppercase. (Or, move them into static members)
//TODO: delete as many enums as possible.

#ifndef _NS73_H
#define _NS73_H

#include <Arduino.h>

enum	//R0 bit values
{
	PE   = 0,
	PDX  = 1,
	MUTE = 2,
	EM   = 4,
	EMS  = 5,
	AG0  = 6,
	AG1  = 7
};

static const uint8_t MAX_REG = 9;

//EEPROM constants
static const uint8_t epMagicOffset = 0x95;
static const uint8_t epMagic = 0x56;
static const uint8_t epCEXTableOffset = 0x96;

//Default values for CEX table in EEPROM.
static const uint8_t epCEXBand0Begins = 71;
static const uint8_t epCEXBand1Begins = 41;
static const uint8_t epCEXBand2Begins = 12;

//Pre-calculated oscillator values baked into flash. The first entry represents 87.5 and values climb in .2 MHz increments.
static const uint8_t MAXCHAN = 103;
const uint16_t channels[MAXCHAN] PROGMEM = 
{
	10718,  10743,  10767,  10792,  10816,  10840,  10865,  10889,  10914,  10938,  10962,  10987,  //89.7
	11011,  11036,  11060,  11084,  11109,  11133,  11158,  11182,  11207,  11231,  11255,  11280,  //92.1
	11304,  11329,  11353,  11377,  11402,  11426,  11451,  11475,  11500,  11524,  11548,  11573,  //94.5
	11597,  11622,  11646,  11670,  11695,  11719,  11744,  11768,  11792,  11817,  11841,  11866,  //96.9
	11890,  11915,  11939,  11963,  11988,  12012,  12037,  12061,  12085,  12110,  12134,  12159,  //99.3
	12183,  12208,  12232,  12256,  12281,  12305,  12330,  12354,  12378,  12403,  12427,  12452,  //101.7
	12476,  12500,  12525,  12549,  12574,  12598,  12623,  12647,  12671,  12696,  12720,  12745,  //104.1
	12769,  12793,  12818,  12842,  12867,  12891,  12916,  12940,  12964,  12989,  13013,  13038,  //106.5
	13062,  13086,  13111,  13135,  13160,  13184,  13208  //107.9
};

class NS73Class
{
private:
	uint8_t reg[MAX_REG];
		
	uint8_t sdo;
	uint8_t sla;
	uint8_t sck;
	uint8_t teb;
	uint8_t initStatus;
		
	uint8_t channel;

	void serialReset(void);
	void softwareReset(void);
	void setRegisterSPI(const uint8_t address, const uint8_t value);
	uint8_t getRegister(const uint8_t which);
	void updateRegister(const uint8_t address, const uint8_t value);
	void changeChannel(const uint8_t chan);
	void setCEX(const uint8_t value);
	void cexSeek(const uint8_t chan);
	uint8_t cexLookup(const uint8_t chan);
	uint8_t haveTEBLock(void);
	uint16_t freqLookup(const uint8_t index);
	void ModifyCEXTable(const uint8_t chan, const uint8_t value);
	void EEPROMWrite(const uint16_t address, const uint8_t value);
	uint8_t EEPROMRead(const uint16_t address);
	uint8_t EEPROMInit(void);
	uint8_t EEPROMValid(void);
	uint8_t EEPROMReset(void);

public:
	NS73Class();
	void begin(const uint8_t dataPin, const uint8_t clockPin, const uint8_t latchPin, const uint8_t tebPin);
	void channelUp(void);
	void channelDown(void);
	void setChannel(const uint8_t to);
	uint8_t getChannel(void);
	void setFrequency(const uint16_t freq);
	uint16_t getFrequency(void);
	void goOnline();
	void goOffline();
	void mute(void);
	void unMute(void);
	void setInputAttenuation(const uint8_t to);
	void setTXPower(const uint8_t to);
};

extern NS73Class NS73;

#endif

