//Sample implementation of the NS73 Arduino driver
//by Conor Peterson, 2012 (conor.p.peterson@gmail.com)

#include <EEPROM.h>
#include "NS73.h"

const byte channelOffset = 0x20;
const byte defaultFrequency = 875;  //87.5 MHz.

const byte onAirIndicator = 13;
const byte ns73DataPin = 2;
const byte ns73ClockPin = 3;
const byte ns73LatchPin = 4;
const byte ns73TEBPin = 5;
const byte downButton = 8;
const byte upButton = 9;

unsigned int resetCounter = 0;

void setup(void)
{
  byte channel;

  pinMode(onAirIndicator, OUTPUT);
  pinMode(downButton, INPUT);
  pinMode(upButton, INPUT);

  //Enable internal pull-up resistors for the channel buttons.
  digitalWrite(downButton, HIGH);
  digitalWrite(upButton, HIGH);

  NS73.begin(ns73DataPin, ns73ClockPin, ns73LatchPin, ns73TEBPin);
  
  channel = EEPROM.read(channelOffset);

  if( channel >= NS73.getMaxChannel() )  //Likely if the EEPROM is uninitialized.
    NS73.setFrequency(defaultFrequency);
  else
    NS73.setChannel(channel);

  NS73.goOnline();
}

void loop(void)
{  
  byte down = digitalRead(downButton);
  byte up = digitalRead(upButton);
    
  if(NS73.onAir() )
    digitalWrite(onAirIndicator, HIGH);
  else
    digitalWrite(onAirIndicator, LOW);

  //In this basic implementation, Up moves the frequency up one channel, Down functions similarly.
  //If both buttons are held a reset counter is incremented. When the counter reaches a high number,
  //the channel is reset to default. Button debouncing is accomplished through software delays.
  if( down == LOW && up == LOW )
  {
    resetCounter++;
    if( resetCounter == 32000 )
    {
      resetCounter = 0;
      NS73.setFrequency(defaultFrequency);
      digitalWrite(onAirIndicator, LOW);
      delay(3000);
      saveChannel();
    }
  }
  else
  {
    resetCounter = 0;

    if( down == LOW )
    {
      if( NS73.channelDown() )
      {
        digitalWrite(onAirIndicator, LOW);
        saveChannel();
      }
      delay(250);
    }
    else if( up == LOW )
    {
      if( NS73.channelUp() )
      {
        digitalWrite(onAirIndicator, LOW);
        saveChannel();
      }
      delay(250);
    }
  }
}

void saveChannel(void)
{
  byte channel = NS73.getChannel();

  //Only write to EEPROM if it actually needs to be changed.
  if( EEPROM.read(channelOffset) != channel )
    EEPROM.write(channelOffset, channel);
}



