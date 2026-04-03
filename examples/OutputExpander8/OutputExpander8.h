#pragma once

/////////////////////////////////////////////////////////////////////////
// Using a 74HC595 8-bit shift register as an 8-bit Output Expander
// mumanchu + muman.ch, 2025.04.02
// https://github.com/mumanchu/OptimizedGPIO
/*
This was tested with a 74HC595, but it should work with other chips too.

	dataPin   = SER
	clockPin  = SRCLK
	strobePin = RCLK
	Connect /OE to GND, connect /SRCLR to VCC.

VCC is 3.3V or 5V according to your microcontroller.

>>> DO NOT CONNECT VCC TO 5V IF YOUR MCU IS 3.3V! <<<
See Disclaimer.
*/

#include "OptimizedGPIO.h"

class OutputExpander8
{
protected:
	OptimizedGPIO data;
	OptimizedGPIO clock;
	OptimizedGPIO strobe;
public:
	bool begin(uint dataPin, uint clockPin, uint strobePin);
	void writeByte(byte b);

	// Optional methods for timing and testing
	void writeByteSlow(byte b);
	bool shiftOutTest(uint readPin);
};

bool OutputExpander8::begin(uint dataPin, uint clockPin, uint strobePin)
{
	if (!data.begin(dataPin, OUTPUT))
		return false;
	data.reset();
	if (!clock.begin(clockPin, OUTPUT))
		return false;
	clock.reset();
	if (!strobe.begin(strobePin, OUTPUT))
		return false;
	strobe.reset();
	return true;
}

// Clock out a byte using fast OptimizedGPIO
void OutputExpander8::writeByte(byte b)
{
	for (uint mask = 0x80; mask; mask >>= 1) {
		// set up the data bit, MS bit first
		data.write(b & mask);
		// clock it in
		clock.set();
		clock.reset();
	}

	// strobe the 8 bits to the outputs
	strobe.set();
	strobe.reset();
}

// The slow version using digitalWrite(), for timing comparison
void OutputExpander8::writeByteSlow(byte b)
{
	for (uint mask = 0x80; mask; mask >>= 1) {
		digitalWrite(data.getPin(), b & mask);
		digitalWrite(clock.getPin(), 1);
		digitalWrite(clock.getPin(), 0);
	}
	digitalWrite(strobe.getPin(), 1);
	digitalWrite(strobe.getPin(), 0);
}

// This test compares the byte shifted out with the byte that was 
// previously shifted in. It reads the chip's 'shift out' pin. 
// As each bit is shifted in, an existing bit is shifted out.
// The shifted-out can be used for chaining multiple chips together.
// The byte that's shifted out is compared with the previously 
// written byte.
// It needs an additional input for the chip's shiftOutPin.
bool OutputExpander8::shiftOutTest(uint shiftOutPin)
{
	OptimizedGPIO shift;
	if(!shift.begin(shiftOutPin, INPUT))
		return false;

	// run the test with 1000 random bytes
	byte blast = 0;

	for (int i = 0; i < 1000; ++i) {
		byte btx = rand();
		byte brx = 0;

		// clock in the byte
		// create a new byte from each bit that's shifted out
		for (uint mask = 0x80; mask; mask >>= 1) {
			// read the bit that's been shifted out
			if (shift.read())
				brx |= mask;
			// set up the next data bit, MS bit first
			data.write(btx & mask);
			// and clock it in 
			clock.set();
			clock.reset();
		}

		// compare the byte that was shifted out with the last byte
		if (brx != blast && i != 0) {
			// compare failed
			return false;
		}

		// save the last byte to compare with the next byte shifted out
		blast = btx;
	}
	return true;		// it worked!
}

