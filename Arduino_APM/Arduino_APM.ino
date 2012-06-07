#include <EEPROM.h>
#include "NS73.h"

int onAirIndicator = 13;
int ns73DataPin = 2;
int ns73ClockPin = 3;
int ns73LatchPin = 4;
int ns73TEBPin = 5;
int channelDown = 8;
int channelUp = 9;

static const int channelOffset = 0x20;

void setup()
{
  uint8_t channel;

  pinMode(onAirIndicator, OUTPUT);
  pinMode(channelDown, INPUT);
  pinMode(channelUp, INPUT);

  //Enable internal pull-up resistors for the channel buttons.
  digitalWrite(channelDown, HIGH);
  digitalWrite(channelUp, HIGH);

  NS73.begin(ns73DataPin, ns73ClockPin, ns73LatchPin, ns73TEBPin);
  
  channel = EEPROM.read(channelAddress);

  if( channel == 255 )  //Uninitialized
    NS73.setFrequency(875);
  else
    NS73.setChannel(channel);

  NS73.goOnline();
}

void loop()
{
  if(NS73.onAir() )
    digitalWrite(onAirIndicator, HIGH);
  else
    digitalWrite(onAirIndicator, LOW);

  if( digitalRead(channelDown) == LOW )
  {
    NS73.channelDown();
    digitalWrite(onAirIndicator, LOW);
    saveChannel();
    delay(100);
  }

  if( digitalRead(channelUp) == LOW )
  {
    NS73.channelUp();
    digitalWrite(onAirIndicator, LOW);
    saveChannel();
    delay(100);
  }
}

void saveChannel()
{
  byte channel = NS73.getChannel();

  //Only write to EEPROM if it's actually being changed.
  if( EEPROM.read(channelOffset) != channel )
    EEPROM.write(channelOffset, channel);
}



