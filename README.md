Arduino driver for the NS73M FM transmitter IC
By Conor Peterson, 2012 (conor.p.peterson@gmail.com)

This is an Arduino driver for the Niigata Seimitsu NS73M low-power FM transmitter. It implements frequency selection and most other interesting features of the chip including adjustable input sensitivity and transmission power, muting, and the ability to take the transmitter offline. At present it uses SPI to communicate with the NS73M, though I2C support may be possible in the future. It requires approximately 32 bytes of RAM and 30 bytes of EEPROM to store calibration information. You can obtain one of these on a breakout board from Sparkfun, product number #. The frequency range is 87.5 to 107.9 MHz.

For instructions and more detailed technical information, please see readme.html document and the comment block on NS73.h.

Most features have been tested and are confirmed working on Uno and Duemilanove boards under Arduino 1.0.1. Bug fixes, suggestions, forks, etc. welcome.

This free software is licensed for use under the terms of the GNU General Public License, version 3.