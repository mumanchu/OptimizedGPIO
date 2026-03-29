# FastGPIO

This library is a single include file that provides fast Digital I/O for STM32, SAMD, AVR, ESP32 and ESP8266 MCUs. The code from the MCU is selected automatically by `#ifdef` directives, so you don't need to do anything special.

## Why do we need faster I/O?

The traditional `digitalRead()` and `digitalWrite()` methods are quite slow when compared to code that accesses the microcontroller's GPIO registers directly.

The reason for this is that a lot of work is done for each access which could be done by `begin()`. This saves a lot of time when the program is running, and is particularly important in an interrupt handler (ISR) which must be as short as possible.

Here is the standard Arduino code for `digitalWrite()`. The lines marked with '*' can be called in `begin()`. The lines marked with '**' are not needed if you are sure the output is not a PWM output. 

```cpp
void digitalWrite(uint8_t pin, uint8_t val)
{
	uint8_t timer = digitalPinToTimer(pin);    //**
	uint8_t bit = digitalPinToBitMask(pin);    //*
	uint8_t port = digitalPinToPort(pin);      //*
	volatile uint8_t *out;

	if (port == NOT_A_PIN) return;

	// If the pin that support PWM output, we need to turn it off
	// before doing a digital write.
	if (timer != NOT_ON_TIMER) turnOffPWM(timer);  //**

	out = portOutputRegister(port);          //*

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
The above code does a read-modify-write, which is why it must disable interrupts while updating the output. SREG is the AVR's Status Register which contains the 'Global Interrupt Enable' bit. It first saves the register with `uint8_t oldSREG = SREG;`, clears the bit (disables interrupts) with `cli()` (same as `noInterrupts()`), then restores the global interrupt state with `SREG = oldSREG;`. This is good, because it leaves the interrupt state in the same state it was before, either enabled or disabled. 

If you used `sti()` or `interrupts()` after updating the output then it would enable interrupts as a side-effect. This is very bad if you want them to be disbled, and can be fatal in an interrupt handler which expects interrupts to be disabled.

The problem with read-modify-write is that an interrupt might occur between the read and the write which changes the register that was just read, and the subsequent write will cause those changes to be lost. This is the problem with the toggle() methods.

The `toggle()` methods in this library have patched-out code to prevent this happening. You don't need this code if you _know_ that output is _never_ modified by an interrupt, which is why it's patched out. Disabling/enabling interrupts also slows it down, so there's no point in doing it if you don't need it.

Some MCUs, like the SAMD, have special registers where you can write just one bit to set or clear a digital output, e.g. OUTSET and OUTCLR. This is great because it does not need a dangerous (and slower) read-modify-write to set an output and you don't need to disable interrupts.



TODO add nointerrupts and restoreinterrupts?

## Timing Comaprisons

This table compares the standard `digitalRead()` and `digitWrite()` calls with the FastGPIO methods for each MCU type and a couple of different clock speeds. You can see it makes a huge difference, especially if you are doing something like "bit-banging" multiple bits into a shift register - a nice way to extend the number of outputs, see ???


## Using the Library




## Example Sketch





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


## Using FastGPIO to bit-bang a serial shift register

This is a nice application of FastGPIO...







```
