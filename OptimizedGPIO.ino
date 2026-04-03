////////////////////////////////////////////////////////////////////////////
// OptimizedGPIO Library Example
// 
// This example outputs GPIO timings to Serial, comparing the execution 
// times of digitalRead(), digitalWite(), OptimisedGPIO.read() and 
// OptimizedGPIO.set(). The timings are in microseconds for 100000 calls.
// It can also show the timings for digitalReadFast() and digitalWriteFast().
// 
// Open the 'Serial Monitor' at 115200 baud to see the output.
// 
// mumanchu + muman.ch, 2026.04.03
// https://github.com/mumanchu/OptimizedGPIO

#include "OptimizedGPIO.h"

// The pins to be used for testing
#define TEST_OUTPUT_PIN LED_BUILTIN
#define TEST_INPUT_PIN 3
#if TEST_OUTPUT_PIN == TEST_INPUT_PIN
#error TEST_OUTPUT_PIN == TEST_INPUT_PIN
#endif
OptimizedGPIO testOutputPin;
OptimizedGPIO testInputPin;

// The number of times around the timing loop
#define LOOP_COUNT 100000

void setup() 
{
	Serial.begin(115200);

	// delay to give you time to open the serial monitor
	delay(3000);

	Serial.println("\n\rStarted...\n\r");
	Serial.flush();

	// initialize the test pins for optimized IO
	if (!testOutputPin.begin(TEST_OUTPUT_PIN, OUTPUT)) {
		Serial.println("FATAL ERROR: testOutputPin.begin() failed");
		Serial.flush();
		while (1);
	}
	if (!testInputPin.begin(TEST_INPUT_PIN, INPUT)) {
		Serial.println("FATAL ERROR: testInputPin.begin() failed");
		Serial.flush();
		while (1);
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

	sprintf(buf, "loopTime       = %lu microseconds", loopTime);
	Serial.println(buf);
	Serial.flush();		// call this so background comms doesn't affect the timing

	// time digitalRead()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalRead(TEST_INPUT_PIN);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalRead()  = %lu microseconds", elapsedTime);
	Serial.println(buf);
	Serial.flush();

	// time digitalWrite()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalWrite(TEST_OUTPUT_PIN, 1);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalWrite() = %lu microseconds", elapsedTime);
	Serial.println(buf);
	Serial.flush();


	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	// Patch this out if digitalReadFast() and digitalWriteFast() do not exist
	#if 0
	// time digitalReadFast()
	PinName pinName = digitalPinToPinName(TEST_INPUT_PIN);
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalReadFast(pinName);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalReadFast()  = %lu microseconds", elapsedTime);
	Serial.println(buf);
	Serial.flush();

	// time digitalWriteFast()
	pinName = digitalPinToPinName(TEST_OUTPUT_PIN);
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		digitalWriteFast(pinName, 1);
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "digitalWriteFast() = %lu microseconds", elapsedTime);
	Serial.println(buf);
	Serial.flush();
	#endif
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


	// time testPin.read()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		testInputPin.read();
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "read()         = %lu microseconds", elapsedTime);
	Serial.println(buf);
	Serial.flush();

	// time testPin.set()
	t = micros();
	for (volatile long i = 0; i < LOOP_COUNT; ++i) {
		testOutputPin.set();
	}
	elapsedTime = micros() - t - loopTime;
	sprintf(buf, "set()          = %lu microseconds", elapsedTime);
	Serial.println(buf);

	Serial.println();
	Serial.flush();

	delay(1000);
}
