# FastGPIO

This library is a single include file that provides fast digital I/O for STM32, SAMD, AVR, ESP32 and ESP8266 MCUs. The code for the MCU is selected automatically by `#ifdef` directives, so you don't need to do anything special.

This voluminous README text (sorry) is aimed at beginners. The rest of us probably know this stuff already.


## Why do we need faster I/O?

The traditional `digitalRead()` and `digitalWrite()` functions are quite slow when compared to stripped-down code that accesses the microcontroller's GPIO registers directly.

The reason for this is that a lot of work is done for each access which could be done in a `begin()` method. This would save a lot of time when the program is running. Fast IO is particularly important in an interrupt handler (ISR), which must be as short as possible.

Here is the standard Arduino code for `digitalWrite()`. The lines marked with '*' can be called in `begin()`. The lines marked with '**' are not needed if you are sure the output is not a PWM output. So most of the code can be transferred to `begin()`, leaving only the code that directly accesses the MCU's GPIO output register.

```cpp
void digitalWrite(uint8_t pin, uint8_t val)
{
	uint8_t timer = digitalPinToTimer(pin); //**
	uint8_t bit = digitalPinToBitMask(pin); //*
	uint8_t port = digitalPinToPort(pin);   //*
	volatile uint8_t *out;

	if (port == NOT_A_PIN) return;  //*

	// If the pin that support PWM output, we need to turn it off
	// before doing a digital write.
	if (timer != NOT_ON_TIMER) turnOffPWM(timer); //**

	out = portOutputRegister(port); //*

	uint8_t oldSREG = SREG;
	cli();

	if (val == LOW) {
		*out &= ~bit;
	} else {
		*out |= bit;
	}

	SREG = oldSREG;
}
```
The above code does a read-modify-write, which is why it must disable interrupts while updating the output. SREG is the AVR's Status Register which contains the 'Global Interrupt Enable' bit. It first saves the SREG register with `uint8_t oldSREG = SREG;`, clears the bit (disables interrupts) with `cli()` (same as `noInterrupts()`), then restores the global interrupt state with `SREG = oldSREG;` afterwards. This is good, because it leaves the interrupt state in the same state it was in before, either enabled or disabled. 

If you used `sti()` or `interrupts()` instead of `SREG = oldSREG;` then it would enable interrupts as a side-effect. This is very bad if you want them to remain disabled, and can be fatal in an interrupt handler which expects interrupts to be disabled until it returns.

The problem with read-modify-write is that an interrupt might occur _between_ the read and the write which changes the register that was just read, and the subsequent write will cause those changes to be overwritten. 

This is also a problem with the `toggle()` methods. The `toggle()` methods in this library have patched-out code to prevent this happening. You don't need this code if you _know_ that the output is _never_ modified by an interrupt, which is why it is patched out. Disabling/enabling interrupts also slows it down, so there's no point in doing it if you don't need it.

Some MCUs, like the SAMD, have special registers where you can write just one bit to set or clear a digital output, e.g. OUTSET and OUTCLR. This is great because it does not need a dangerous (and slower) read-modify-write operation to set an output, and you don't need to disable interrupts.



TODO add nointerrupts and restoreinterrupts?

## Timing Comparisons

This table compares the standard `digitalRead()` and `digitWrite()` calls with the FastGPIO methods for some MCU types and some different clock speeds. You can see it makes a huge difference. This effect is accumulative if you are doing something intensive like "bit-banging" multiple bits into a shift register.

IO read/write timing

| Method           | STM32 xMHz | SAMD21 xMHz | SAMD51 xMHz | ATxxx xMHz | ESP32 xMHz | 
| :---------       | :--------- | :---------  | :---------  | :--------- | :--------- |
| digitalRead()    |            |             |             |            |            |
| FastGPIO.read()  |            |             |             |            |            |
| digitalWrite()   |            |             |             |            |            |
| FastGPIO.write() |            |             |             |            |            |


Bit-banging a shift register timing

TODO


## Using the Library

TODO


## Example Sketch

TODO

<details>
<summary>Click to see the code</summary>

###

```cpp
#pragma once

// Fast Digital I/O for STM32, SAMD, AVR, ESP32, ESP8266 
// Fast replacements for the rather slow digitalRead() and digitalWrite()
// Uses direct access to the MCU's GPIO registers
// muman.ch, 2026.02.26
// email: info@muman.ch
// 
// All methods are inline except for begin().
// For speed, no error checking is done except by pinMode() in begin()
// (which may or may not be reported, and may go into InfiniteLoop)
// 
// Do not write to inputs or PWM outputs, the behaviour is undefined!

// If outputs on a port can be modified by an interrupt, toggle() and
// other methods which do read-modify-write must disable interrupts,
// see commented out code below. But this makes them slower.

#if defined(STM32_CORE_VERSION)

// STM32
// C:\Users\...\AppData\Local\Arduino15\packages\STMicroelectronics\hardware\stm32\2.12.0\cores\arduino\wiring_digital.c

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	GPIO_TypeDef* port;
	uint mask;
public:
	void begin(uint pin, uint mode);
	bool read() { return (port->IDR & mask) != 0; }
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set() { port->BSRR = mask; }
	void reset() { port->BSRR = mask << 16; }
	void toggle();
};

void FastGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);
	port = digitalPinToPort(pin);
	mask = digitalPinToBitMask(pin);
}

inline void FastGPIO::toggle()
{
	//TODO uncomment the interrupt save-disable-restore code
	//     if output states can be changed by an interrupt
	//bool interruptsEnabled = !(__get_PRIMASK() & 1);
	//noInterrupts();
	port->ODR ^= pin;
	//if (interruptsEnabled)
	//	interrupts();
}

#elif defined(_SAMD21_) || defined(_SAMD51_)

// SAMD
// C:\Users\...\AppData\Local\Arduino15\packages\arduino\hardware\samd\1.8.14\cores\arduino\wiring_digital.c

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	ulong mask;
	PortGroup* port;
public:
	void begin(uint pin, uint mode);
	bool read() { return (port->IN.reg & mask) != 0; }
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set() { port->OUTSET.reg = mask; }
	void reset() { port->OUTCLR.reg = mask; }
	void toggle() { read() ? reset() : set(); }
};

void FastGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);
	mask = 1ul << g_APinDescription[pin].ulPin;
	port = PORT->Group + g_APinDescription[pin].ulPort;
}

#elif defined(ESP32)

// ESP32

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	uint32_t mask;
	uint32_t ts_reg;
	uint32_t tc_reg;
	uint32_t in_reg;
public:
	void begin(uint pin, uint mode);
	bool read() { return REG_GET_BIT(in_reg, mask) != 0; }
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set()    { REG_WRITE(ts_reg, mask); }
	void reset()  { REG_WRITE(tc_reg, mask); }
	void toggle() { read() ? reset() : set(); }
};

void FastGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);

	if (pin < 32) {
		mask = 1LU << pin;
		ts_reg = GPIO_OUT_W1TS_REG;
		tc_reg = GPIO_OUT_W1TC_REG;
		in_reg = GPIO_IN_REG;
	}
	// some don't have more than 32 pins
	#ifdef GPIO_OUT1_W1TS_REG
	else {
		mask = 1LU << (pin - 32);
		ts_reg = GPIO_OUT1_W1TS_REG;
		tc_reg = GPIO_OUT1_W1TC_REG;
		in_reg = GPIO_IN1_REG;
	}
	#endif
}

#elif defined(ESP8266)

// ESP8266
// C:\Users\...\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\cores\esp8266\core_esp8266_wiring_digital.cpp

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	ulong mask;
public:
	void begin(uint pin, uint mode);
	bool read();
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set();
	void reset();
	void toggle() { read() ? reset() : set(); }
};

void FastGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);
	mask = (pin < 16) ? (1lu << pin) : 0;
}

inline bool FastGPIO::read()
{
	if (pin < 16)
		return (GPI & mask) != 0;	// #define GPIP(p) ((GPI & (1 << ((p) & 0xF))) != 0)
	if (pin == 16)
		return GP16I & 0x01;
	return 0;
}

inline void FastGPIO::set()
{
	if (pin < 16)
		GPOS = mask;
	else if (pin == 16) {
		//TODO uncomment the interrupt save-disable-restore code
		//     if output states can be changed by an interrupt
		//ulong savedPS = xt_rsil(15);	// save interrupt state and disable all interrupts
		GP16O |= 1;
		//xt_wsr_ps(savedPS);			// restore interrupt state
	}
}

inline void FastGPIO::reset()
{
	if (pin < 16)
		GPOC = mask;
	else if (pin == 16) {
		//TODO uncomment the interrupt save-disable-restore code
		//     if output states can be changed by an interrupt
		//ulong savedPS = xt_rsil(15);	// save interrupt state and disable all interrupts
		GP16O &= ~1;
		//xt_wsr_ps(savedPS);			// restore interrupt state
	}
}

#elif defined(__AVR_ARCH__)

// AVR, e.g. Atmega328, Atmega2560, etc
// C:\Users\...\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.7\cores\arduino\wiring_digital.c

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
	byte mask;
	byte* outreg;
	byte* inreg;
public:
	void begin(uint pin, uint mode);
	bool read() { return (*inreg & mask) != 0; }
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set();
	void reset();
	void toggle() { read() ? reset() : set(); }
};

void FastGPIO::begin(uint pin, uint mode)
{
	this->pin = pin;
	pinMode(pin, mode);
	mask = digitalPinToBitMask(pin);
	byte port = digitalPinToPort(pin);
	outreg = (byte*)portOutputRegister(port);
	inreg = (byte*)portInputRegister(port);
}

inline void FastGPIO::set()
{
	//TODO uncomment the interrupt save-disable-restore code
	//     if output states can be changed by an interrupt
	//byte oldSREG = SREG;		// save interrupt state
	//noInterrupts();			// disable all interrupts
	*outreg |= mask;
	//SREG = oldSREG;			// restore interrupt state
}

inline void FastGPIO::reset()
{
	//TODO uncomment the interrupt save-disable-restore code
	//     if output states can be changed by an interrupt
	//byte oldSREG = SREG;		// save interrupt state
	//noInterrupts();			// disable all interrupts
	*outreg &= ~mask;
	//SREG = oldSREG;			// restore interrupt state
}

#else 

// Unknown microcontroller, use the slow digitalRead() and digitalWrite()

#warning "Unknown microcontroller, needs FastGPIO class. Email info@muman.ch and we'll add it."

class FastGPIO
{
	uint pin = PNUM_NOT_DEFINED;
public:
	void begin(uint pin, uint mode) { this->pin = pin; pinMode(pin, mode); }
	bool read() { return digitalRead(pin); }
	// do not use these with INPUTs
	void write(bool b) { b ? set() : reset(); }
	void set() { digitalWrite(pin, 1); }
	void reset() { digitalWrite(pin, 0); }
	void toggle() { digitalWrite(pin, !digitalRead(pin)); }
};

#endif
```
</details>


## Using FastGPIO to bit-bang a serial shift register

This is a nice application for FastGPIO and it makes huge difference.

The cheapest way to extend the number of outputs of your MCU is to add a 'serial shift register' chip. This is a 'serial in parallel out' chip which lets you clock in bits serially, then transfer them to an 8 or 16-bit output register.

You need 3 outputs to write to it, DATA, CLOCK and STROBE (although they may have different names). CLOCK clocks in a DATA bit on its rising edge. After 8 or 16 bits have been clocked in, STROBE transfers the data to the outputs.

There are several versions of this chip, such as 74HC595, CD4095, ...

TODO chip details

TODO source code


<details>
<summary>Click to see the code</summary>

###

```cpp
// Example using a 74HC595 8-bit shift register as an Output Expander
// muman.ch, 2025.06.21

#if 0
// Fast Version 
// This works only on the STM32F103xB MCU
// On a 72MHz STM32F103 the takes 9.2uS to write 8 bits
// The bit-bang clock runs at 1.18MHz

#include <stm32f103xb.h>
#include <stm32f1xx_hal_gpio.h>


class Encoder74HC595
{
protected:
	#define SET_PIN(port, pin)     ((port)->BSRR = (pin))
	#define CLEAR_PIN(port, pin)   ((port)->BSRR = (pin << 16))
	#define READ_PIN(port, pin)    ((port)->IDR & (pin))
	//#define TOGGLE_PIN(port, pin)  ((port)->ODR  ^= (pin))

	// data bit, 0 or 1
	GPIO_TypeDef* portSER;
	int maskSER;
	// clock shift register to output latches on rising edge
	GPIO_TypeDef* portRCLK;
	int maskRCLK;
	// data clock, data bit clocked into shift register on rising edge
	GPIO_TypeDef* portSRCLK;
	int maskSRCLK;
	// clear shift register on falling edge
	GPIO_TypeDef* portSRCLR;
	int maskSRCLR;
	// output enable, active low
	byte pinOE;

	// current state of the port
	byte curData8 = 0;

public:
	void begin(byte pinSER, byte pinRCLK, byte pinSRCLK, 
		byte pinSRCLR, byte pinOE = 255);
	void enableOutputs(bool enable);
	void clear();
	void writeBit(byte bit, bool value);
	void writeByte(byte data8);
	bool readBit(byte bit) { return curData8 & (1 << bit); }
	byte readByte() { return curData8; }

	bool test(byte pinQH);
	byte txRx(GPIO_TypeDef* portQH, int naskQH, byte dataOut);
};


void Encoder74HC595::begin(byte pinSER, byte pinRCLK, 
	byte pinSRCLK, byte pinSRCLR, byte pinOE)
{
	// clear shift register, falling edge
	portSRCLR = digitalPinToPort(pinSRCLR);
	maskSRCLR = digitalPinToBitMask(pinSRCLR);
	pinMode(pinSRCLR, OUTPUT);
	CLEAR_PIN(portSRCLR, maskSRCLR);
	SET_PIN(portSRCLR, maskSRCLR);

	// clock shift register to output latches on rising edge
	portRCLK = digitalPinToPort(pinRCLK);
	maskRCLK = digitalPinToBitMask(pinRCLK);
	pinMode(pinRCLK, OUTPUT);
	CLEAR_PIN(portRCLK, maskRCLK);
	SET_PIN(portRCLK, maskRCLK);
	CLEAR_PIN(portRCLK, maskRCLK);

	// data bit, 0 or 1
	portSER = digitalPinToPort(pinSER);
	maskSER = digitalPinToBitMask(pinSER);
	pinMode(pinSER, OUTPUT);
	CLEAR_PIN(portSER, maskSER);

	// data clock, data bit clocked into shift register on rising edge
	portSRCLK = digitalPinToPort(pinSRCLK);
	maskSRCLK = digitalPinToBitMask(pinSRCLK);
	pinMode(pinSRCLK, OUTPUT);
	CLEAR_PIN(portSRCLK, maskSRCLK);

	// optional output enable, active low
	this->pinOE = pinOE;
	if (pinOE < 255)
		pinMode(pinOE, OUTPUT);
}

// Enable/disable outputs
void Encoder74HC595::enableOutputs(bool enable)
{
	if (pinOE < 255)
		digitalWrite(pinOE, !enable);
}

// Clear all outputs
void Encoder74HC595::clear()
{
	// clear shift register on falling edge
	CLEAR_PIN(portSRCLR, maskSRCLR);
	SET_PIN(portSRCLR, maskSRCLR);

	// transfer the shift register to the output latches on rising edge
	SET_PIN(portRCLK, maskRCLK);
	CLEAR_PIN(portRCLK, maskRCLK);

	curData8 = 0;
}

// Write all 8 bits to the outputs
void Encoder74HC595::writeByte(byte data8)
{
	curData8 = data8;

	// clock 8 data bits into the shift register, ms bit first
	// bit7 = QH .. bit 0=QA
	for (int mask = 0x80; mask; mask >>= 1) {

		// clock the next data bit into the shift register
		if (data8 & mask)
			SET_PIN(portSER, maskSER);
		else
			CLEAR_PIN(portSER, maskSER);
		SET_PIN(portSRCLK, maskSRCLK);
		CLEAR_PIN(portSRCLK, maskSRCLK);
	}

	// transfer the shift register to the output latches on rising edge
	SET_PIN(portRCLK, maskRCLK);
	CLEAR_PIN(portRCLK, maskRCLK);
}

// Write a single bit to one output
void Encoder74HC595::writeBit(byte bit, bool value)
{
	if (bit > 7)
		return;
	int mask = 1 << bit;
	byte curData;
	if (value)
		curData = curData8 | mask;
	else
		curData = curData8 & ~mask;
	writeByte(curData);
}


// Shift Test
// Tests the byte shifted out of QH is the same as the byte
// that was previously shifted in.
// Output states are not changed.
// Requires an extra input connection to the chip's QH pin.
bool Encoder74HC595::test(byte pinQH)
{
	GPIO_TypeDef* portQH = digitalPinToPort(pinQH);
	int maskQH = digitalPinToBitMask(pinQH);

	byte b1 = rand();
	byte b2 = rand();
	byte b3 = txRx(portQH, maskQH, b1);
	byte b4 = txRx(portQH, maskQH, b2);
	byte b5 = txRx(portQH, maskQH, b3);

	return b4 == b1 && b5 == b2;
}

byte Encoder74HC595::txRx(GPIO_TypeDef* portQH, int maskQH, byte dataOut)
{
	byte dataIn = 0;
	for (int mask = 0x80; mask; mask >>= 1) {
		// read the value shifted out of the QH bit
		if (READ_PIN(portQH, maskQH))
			dataIn |= mask;
		// clock the next data bit into the shift register
		if (dataOut & mask)
			SET_PIN(portSER, maskSER);
		else
			CLEAR_PIN(portSER, maskSER);
		SET_PIN(portSRCLK, maskSRCLK);
		CLEAR_PIN(portSRCLK, maskSRCLK);
	}
	return dataIn;
}

Encoder74HC595 enc;


void setup()
{
	Serial.begin(115200);
	delay(3000);
	Serial.println("\nMCU Started...");

	enc.begin(D7, D6, D5, D4);
}

byte data = 0xa5;

void loop()
{
	enc.writeByte(data);
	data = ~data;

	if (!enc.test(D3)) {
		Serial.println("failed");
		Serial.flush();
	}
}


#else

// Conventional Slow Version
// This should work on all devices
// On a 72MHz STM32F103, this takes 46.2uS to write 8 bits
// The bit-bang clock runs at 197.3KHz

#define SER   D7	// data bit, 0 or 1
#define RCLK  D6	// clock shift register to output latches, rising edge
#define SRCLK D5	// data clock, data bit clocked into shift register on rising edge
#define SRCLR D4	// clear shift register, falling edge

void setup()
{
	pinMode(RCLK, OUTPUT);
	digitalWrite(RCLK, 0);

	pinMode(SRCLK, OUTPUT);
	digitalWrite(SRCLK, 0);

	pinMode(SER, OUTPUT);
	digitalWrite(SRCLK, 0);

	pinMode(SRCLR, OUTPUT);
	digitalWrite(SRCLR, 0);
	digitalWrite(SRCLR, 1);
}

byte data = 0xa5;

void loop() 
{
	// clock 8 data bits into the shift register, ms bit first
	// bit7 = QH .. bit 0=QA
	for (int mask = 0x80; mask; mask >>= 1) {
		// write data bit
		digitalWrite(SER, data & mask ? 1 : 0);
		// clock the data bit into the shift register
		digitalWrite(SRCLK, 1);
		digitalWrite(SRCLK, 0);
	}

	// transfer the shift register to the output latches
	digitalWrite(RCLK, 1);
	digitalWrite(RCLK, 0);

	// invert the data and repeat
	data = ~data;
}

#endif
```
</details>



