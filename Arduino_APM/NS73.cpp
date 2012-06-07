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
*/

//NOTE! For instructions and more information, please examine NS73.h.

#include "NS73.h"

NS73Class NS73;

//TESTED:
//goOnline(), mute(), setInputAttenuation() functions tested and working.
//CEX eeprom storage and retrieval is working.
//cexSeek() works -- and comes up with some different values than the default range, which is OK.

//TODO: Test selectable transmit power.
//TODO: Support SPI as well as TWI. (Possible by the user providing a serial out routine?)
//TODO: Verify that all checks against initStatus are meaningful. If they aren't, strip that bullshit out.
//TODO: Can I take out that dumb hack for updateRegister() to catch software reset?
//TODO: Refactor eeprom cex variable names
//TODO: What does pilot tone do?
//TODO: Remove sanity checks on private functions; ensure sanity checks on public functions

NS73Class::NS73Class(void)
{
	sdo = 255;
	sla = 255;
	sck = 255;
	teb = 255;
	
	channel = 0;
	
	reg[0] = 0;
	reg[1] = 0xB4;
	reg[2] = 0x7;
	reg[3] = 0;
	reg[4] = 0;
	reg[5] = 0;
	reg[6] = 0x1E;
	reg[7] = 0;
	reg[8] = 0x1B;
	
	initStatus = 0;
}

void NS73Class::begin(const uint8_t dataPin, const uint8_t clockPin, const uint8_t latchPin, const uint8_t tebPin)
{
	uint8_t i;
	
	sdo = dataPin;
	sck = clockPin;
	sla = latchPin;
	teb = tebPin;
    
	pinMode(sdo, OUTPUT);
	pinMode(sck, OUTPUT);
	pinMode(sla, OUTPUT);
	pinMode(teb, INPUT);
	digitalWrite(teb, LOW);  //Ensure pull-up is disabled. The NS73 will drive this line.
	
	if( !EEPROMValid() )
		EEPROMReset();

	channel = 0;
	
	initStatus = 1;
	
	serialReset();	
	softwareReset();

	for(i = 0; i < MAX_REG; i++ )
		updateRegister(i, reg[i]);
    
	initStatus = 2;
}

//Bring the transmitter online. A required call.
//TODO: Clean up, remove bitvectors and is this preemphasis stuff necessary?
void NS73Class::goOnline()
{
	//Reassert registers 1 and 2
  	updateRegister(1, reg[1] ); //Forced subcarrier, pilot tone on.
	updateRegister(2, reg[2] );	//Unlock detect on, TX power at whatever.
	updateRegister(0, reg[0] | _BV(PE) | _BV(EMS)); //Power on, crystal on, pre-emphasis on and at 75µS  //DEBUG: CHECK
}

//Take transmitter offline. Should save a little power. Implemented by zeroing
//out the last two bits of R0, which disconnects the clock (halting the
//digital circuit) and pulls the plug on the analog circuit.
void NS73Class::goOffline()
{
	updateRegister(0, reg[0] & 0xFC);
}

void NS73Class::changeChannel(const uint8_t chan)
{
	uint16_t freq;
	uint8_t cex;
	uint8_t success;
	
	if( initStatus == 0 || chan >= MAXCHAN )
		return;

	freq = freqLookup(chan);
	cex = cexLookup(chan);

	updateRegister(6, 0x1E ); //Main synth charge pump -> 80µA
	updateRegister(3, (freq & 0xFF));
	updateRegister(4, (freq >> 8));
	setCEX(cex);
	
	//Did we make it online? (Only check if the transmitter is powered up)
	if( !haveTEBLock() && (reg[0] & 0x3) != 0)
	  cexSeek(chan);

	updateRegister(6, 0x1A);	//Main synth charge pump -> 1.25µA
}

//Alter the current CEX setting. It takes a little while for the synthesizer to settle
//so this function only modifies CEX if necessary.
void NS73Class::setCEX(const uint8_t value)
{
	if( value > 3 )
		return;

	if( (reg[8] & 0x3) != value )
	{
		updateRegister(8, (reg[8] & 0xFC) | value );
		delay(175); //TODO: Verify this value.
	}
}

//Pulls a CEX value out of eeprom. The table is packed 4 channels per byte
//so some bitwise arithmetic must be applied.
uint8_t NS73Class::cexLookup(const uint8_t chan)
{
	uint8_t value = EEPROMRead(epCEXTableOffset + (chan >> 2));
	value >>= ((chan & 0x3) << 1);
	return value & 0x3;	
}

//cexSeek() searches for a different CEX setting to make the channel work.
//It assumes the synthesizer is currently unlocked and that the charge pumps are set high.
void NS73Class::cexSeek(const uint8_t chan)
{
	uint8_t newCEX;
	uint8_t oldCEX;
	
	oldCEX = cexLookup(chan);
	
	for( newCEX = 3; newCEX >= 0; newCEX-- )
	{
		setCEX(newCEX);
		delay(500);	//TODO: Determine a better time.
		
		if( haveTEBLock() )
		{
			ModifyCEXTable(chan, newCEX);
			break;
		}		
	}
}

//Apparently needed in both TWI and I2C modes, since it's listed outside of section 7.
void NS73Class::serialReset(void)
{
	uint8_t i;
	
	//Can't check on init variable here here because this call needs to happen early to reset the serial interface.
	//We just check to see if the pins have a value yet.
	if( initStatus == 0)
		return;
	
	//Data and clock begin high
	digitalWrite(sdo, HIGH);
	digitalWrite(sck, HIGH);
	
	//This is the start condition (data transitions to low when clock is high)
	digitalWrite(sdo, LOW);
	
	//Now write "1" 26 times
	digitalWrite(sck, LOW);
	digitalWrite(sdo, HIGH);
	
	for(i = 0; i < 26; i++ )
	{
		digitalWrite(sck, HIGH);
		digitalWrite(sck, LOW);
	}
	
	//Send another start condition
	digitalWrite(sck, HIGH);
	digitalWrite(sdo, LOW);
	
	digitalWrite(sck, LOW);
	
	//Finally, a stop condition
	digitalWrite(sck, HIGH);
	digitalWrite(sdo, HIGH);
	
	//Leave the clock low.
	digitalWrite(sck, LOW);
	delayMicroseconds(1000);//DEBUG
}

uint16_t NS73Class::freqLookup(const uint8_t index)
{
	return (index >= MAXCHAN) ? 0 : pgm_read_word(&channels[index]);
}

//Resets the software (register contents are preserved) by writing a special number to register 14.
void NS73Class::softwareReset(void)
{
	updateRegister(14, 0x5);
}

void NS73Class::updateRegister(const uint8_t address, const uint8_t value )
{
	setRegisterSPI(address, value);
	
	if( address < MAX_REG )
		reg[address] = value;
}
				
//Sets a register on the NS73 via SPI (three-wire), least significant byte first
void NS73Class::setRegisterSPI(const uint8_t address, const uint8_t value)
{
	uint8_t out;
	uint8_t i;
	
	if( initStatus == 0 )
		return;
	
	//The first chunk we will send to the NS73 is the address of the setting we wish to modify.
	//This is only 4 bits.
	out = address;

	//Send the address, LSB first.
	for( i = 0; i < 4; i++ )
	{
		if( out & 0x1 )
			digitalWrite(sdo, HIGH);
		else
			digitalWrite(sdo, LOW);
		
		//Strobe clock
		digitalWrite(sck, HIGH);
		digitalWrite(sck, LOW);
		
		//Move on to the next bit.
		out >>= 1;
	}
	
	//Now send the data. This routine is exactly like above but we send a full byte.
	out = value;
	for( i = 0; i < 8; i++ )
	{
		if( out & 0x1 )
			digitalWrite(sdo, HIGH);
		else
			digitalWrite(sdo, LOW);
		
		//Strobe clock
		digitalWrite(sck, HIGH);
		digitalWrite(sck, LOW);
		
		//Move on to the next bit.
		out >>= 1;
	}
	
	//Finally, strobe the latch so that the NS73 knows we're done sending it data.
	digitalWrite(sla, HIGH);
	digitalWrite(sla, LOW);
}

//The "Check TEB routine" on p. 22 of the NS73 datasheet
uint8_t NS73Class::haveTEBLock(void)
{
	uint8_t i;
	uint8_t checkCount = 0;
	
	if( initStatus > 0 )
	{ 
		for(i = 0; i < 50; i++ )
		{
			if( digitalRead(teb) == HIGH )
				checkCount++;
			delayMicroseconds(2500);
		}
	}
	
	return (checkCount > 25);
}

uint8_t NS73Class::getRegister(const uint8_t which)
{
	return reg[which]; 
}

void NS73Class::channelUp(void)
{
	if( channel < MAXCHAN - 1)
		channel += 1;
	
	changeChannel(channel);
}

void NS73Class::channelDown(void)
{
	if( channel > 0 )
		channel -= 1;

	changeChannel(channel);
}

uint8_t NS73Class::getChannel(void)
{
	return channel;
}

//Returns a numeric frequency for use in e.g. driving an LCD display -- 895 = 89.5 MHz, 1077 = 107.7 MHz.
uint16_t NS73Class::getFrequency(void)
{
	return 875 + (channel << 1);
}

void NS73Class::setFrequency(const uint16_t freq)
{
	setChannel( (freq - 875) >> 1);
}

void NS73Class::setChannel(const uint8_t to)
{
	if( to < MAXCHAN )
	{
		channel = to;
		changeChannel(channel);
	}
}

//Mute the broadcast. Does not take the transmitter offline.
void NS73Class::mute(void)
{
	updateRegister(0, reg[0] | 0x4);
}

//Unmute the broadcast. Does not bring the transmitter online.
void NS73Class::unMute(void)
{
	updateRegister(0, reg[0] & 0xFB);
}

//setInputAttenuation: set the input level required for 100% modulation (i.e. full volume on FM band)
//Possible values are 0 for 100mV, 1 for 140mV, 2 for 200mV. Chip defaults to maximum sensitivity.
void NS73Class::setInputAttenuation(const uint8_t to)
{
	if( to < 3 )
		updateRegister(0, (reg[0] & 0x3F) | (to << 6));
}

//setTXPower: set the transmission power.
//Possible values are 1 for .5mW, 2 for 1mW, 3 for 2mW. Real-life transmission power may be much higher (!)
//Chip defaults to the highest setting, 2 mW. //TODO: Test these settings; what about txpower 0?
void NS73Class::setTXPower(const uint8_t to)
{
	if( to > 0 && to < 4 )
		updateRegister(2, (reg[2] & 0xFC) | to);
}

//Since the CEX table is made of packed bits, must read out the old entry
//and modify it before writing it back.
void NS73Class::ModifyCEXTable(const uint8_t chan, const uint8_t value)
{
	uint8_t offset = epCEXTableOffset + (chan >> 2);
	uint8_t shift = (chan & 0x3) << 1;
	uint8_t oldByte = EEPROMRead(offset);
	uint8_t newByte;

	//Clear out the old entry in newByte
	newByte = oldByte & (~(0x3 << shift));
	//OR in the new entry
	newByte |= (value & 0x3) << shift;

	if( newByte != oldByte )
		EEPROMWrite(offset, newByte);
}

//Custom EEPROM routines that should be more resistant to corruption vs. default
//Arduino/AVR library calls that don't seem to accomodate interrupts.
//These are essentially the routines found in section 7.6.3 of the ATMega328P datasheet.
void NS73Class::EEPROMWrite(const uint16_t address, const uint8_t value)
{
	uint8_t ints = SREG & 0x80;	//Take note of whether or not interrupts are disabled.
	cli();	//Disable interrupts for now.
  	while( EECR & _BV(EEPE) );	//Wait until any previous write completes.
	EEARH = (address >> 8);
	EEARL = address & 0xFF;
	EEDR = value;
	EECR |= _BV(EEMPE);  //master program enable
	EECR |= _BV(EEPE);    //start write

	//All done -- now re-enable interrupts (if they were enabled before)
	if( ints != 0 )
		sei();
}

//See comment on NS73Class::EEPROMWrite for more information.
uint8_t NS73Class::EEPROMRead(const uint16_t address)
{
	uint8_t ints = SREG & 0x80;	//Take note of whether or not interrupts are disabled.	
	cli();	//Disable interrupts for now.
	while( EECR & _BV(EEPE) );	//Make sure we're not actually writing to EEPROM presently.
	EEARH = (address >> 8);
	EEARL = (address & 0xFF);
	EECR |= _BV(EERE);

	if( ints != 0 )
		sei();
	
	return EEDR;
}

//Performs a series of logical checks on the EEPROM to verify its integrity.
//TODO: test this function.
uint8_t NS73Class::EEPROMValid(void)
{
	if( EEPROMRead(epMagicOffset) != epMagic )
		return false;
	
	//TODO: Do a checksum of the CEX table (or a checksum of full EEPROM region).
	
	return true;
}

//Totally resets EEPROM.
uint8_t NS73Class::EEPROMReset(void)
{
	uint8_t i;
	uint8_t cexData;
	uint8_t value;
		
	EEPROMWrite(epMagicOffset, epMagic);

	//Populate the CEX table (entries are packed four per byte)
	//byte offset is  (i >> 2)
	//shift within byte is  ((i & 0x3) << 1)
	for(i = 0; i < MAXCHAN; i++ )
	{
		if( i >= epCEXBand0Begins )
			value = 0;
		else if( i >= epCEXBand1Begins )
			value = 1;
		else if( i >= epCEXBand2Begins )
			value = 2;
		else
			value = 3;
		
		cexData |= value << ((i & 0x3) << 1);
		
		//Write it out when the byte is done or when we've run out of channels
		if( (i & 0x3) == 3 || (i == MAXCHAN - 1) )
		{
			EEPROMWrite(epCEXTableOffset + (i >> 2), cexData);
			cexData = 0;
		}
	}
}
