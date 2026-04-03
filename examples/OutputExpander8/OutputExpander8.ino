//////////////////////////////////////////////////////////////////////
// OptimizedGPIO Library Example
// 
// Using an 8-bit serial-in-parallel-out serial shift register chip
// as an Output Expander by bit-banging the control signals using 
// the OptimizedGPIO Library. 
// It compares the speed of the OptimizedGPIO version with the version
// using digitalWrite(). The OptimizedGPIO version is about 10x faster.
// 
// mumanchu + muman.ch, 2026.03.31
// https://github.com/mumanchu/OptimizedGPIO

#include "OutputExpander8.h"

OutputExpander8 expander;

// Pins connected to the shift register chip
#define DATA_PIN	2
#define CLOCK_PIN	3
#define STROBE_PIN	4
#define SHIFT_PIN	5

// The number of times around the timing loop
#define LOOP_COUNT 100000

void setup() 
{
	Serial.begin(115200);

	// delay to give you time to open the serial monitor
	delay(3000);

	Serial.println("\n\rStarted...\n\r");
	Serial.flush();

	if (!expander.begin(DATA_PIN, CLOCK_PIN, STROBE_PIN)) {
		Serial.println("FATAL ERROR: expander.begin() failed");
		Serial.flush();
		while (1) 
			yield();
	}
}

void loop() 
{
	unsigned long t;
	unsigned long elapsedTime;
	char buf[128];

	// time the timing loop so we can subtract it
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		;
	}
	unsigned long loopTime = micros() - t;

	// if the chip's SHIFT OUT pin is connected to an INPUT, we can run the test
	#ifdef SHIFT_PIN
	if (!expander.shiftOutTest(SHIFT_PIN)) {
		Serial.println("FATAL ERROR: expander.shiftOutTest() failed");
		Serial.flush();
		while (1)
			yield();
	}
	Serial.println("expander.shiftOutTest() passed!");
	#endif

	// time writeByte(), uses OptimizedGPIO
	byte b = 0;
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		expander.writeByte(b);
		b = ~b;
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "writeByte()     = %lu microseconds/byte", 
		elapsedTime / LOOP_COUNT);
	Serial.println(buf);

	// time writeByteSlow(), uses digitalWrite()
	b = 0;
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		expander.writeByteSlow(b);
		b = ~b;
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "writeByteSlow() = %lu microseconds/byte", 
		elapsedTime / LOOP_COUNT);
	Serial.println(buf);

	Serial.println();
}
